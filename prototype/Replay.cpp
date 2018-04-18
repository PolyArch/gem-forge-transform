#include "Replay.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include <fstream>

static llvm::cl::opt<std::string> TraceFileName("trace-file",
                                                llvm::cl::desc("Trace file."));

#define DEBUG_TYPE "ReplayPass"
namespace {

llvm::Constant* getOrCreateStringLiteral(
    std::map<std::string, llvm::Constant*>& GlobalStrings, llvm::Module* Module,
    const std::string& Str) {
  if (GlobalStrings.find(Str) == GlobalStrings.end()) {
    // Create the constant array.
    auto Array =
        llvm::ConstantDataArray::getString(Module->getContext(), Str, true);
    // Get the array type.
    auto ElementTy = llvm::IntegerType::get(Module->getContext(), 8);
    auto ArrayTy = llvm::ArrayType::get(ElementTy, Str.size() + 1);
    // Register a global variable.
    auto GlobalVariableStr = new llvm::GlobalVariable(
        *(Module), ArrayTy, true, llvm::GlobalValue::PrivateLinkage, Array,
        ".str");
    GlobalVariableStr->setAlignment(1);
    // Get the address of the first element in the string.
    // Notice that the global variable %.str is also a pointer, that's why we
    // need two indexes. See great tutorial:
    // https://llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction
    //   auto ConstantZero =
    //   llvm::ConstantInt::get(this->Module->getContext(),
    //  llvm::APInt(32, 0));
    auto ConstantZero = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(Module->getContext()), 0, false);
    std::vector<llvm::Constant*> Indexes;
    Indexes.push_back(ConstantZero);
    Indexes.push_back(ConstantZero);
    GlobalStrings[Str] = llvm::ConstantExpr::getGetElementPtr(
        ArrayTy, GlobalVariableStr, Indexes);
  }
  return GlobalStrings.at(Str);
}
}  // namespace

ReplayTrace::ReplayTrace()
    : llvm::FunctionPass(ID),
      Trace(nullptr),
      OutTraceName("llvm_trace_gem5.txt") {}

ReplayTrace::~ReplayTrace() {
  // Remember to release the trace.
  if (this->Trace != nullptr) {
    delete this->Trace;
    this->Trace = nullptr;
  }
}

void ReplayTrace::getAnalysisUsage(llvm::AnalysisUsage& Info) const {
  Info.addRequired<LocateAccelerableFunctions>();
  Info.addPreserved<LocateAccelerableFunctions>();
}

bool ReplayTrace::doInitialization(llvm::Module& Module) {
  this->Module = &Module;

  assert(TraceFileName.getNumOccurrences() == 1 &&
         "Please specify the trace file.");
  // For now do not load in the trace.
  this->Trace = new DynamicTrace(TraceFileName, this->Module);

  DEBUG(llvm::errs() << "Parsed # dynamic insts: "
                     << this->Trace->DyanmicInstsMap.size() << '\n');

  // DEBUG(llvm::errs() << "Parsed # memory dependences: "
  //                    << this->Trace->NumMemDependences << '\n');

  // Generate the transformation of the trace.
  this->TransformTrace();

  // Register the external ioctl function.
  registerFunction(Module);

  return true;
}

bool ReplayTrace::runOnFunction(llvm::Function& Function) {
  auto FunctionName = Function.getName().str();
  DEBUG(llvm::errs() << "FunctionName: " << FunctionName << '\n');

  // Check if this function is accelerable.
  bool Accelerable =
      getAnalysis<LocateAccelerableFunctions>().isAccelerable(FunctionName);
  // A special hack: do not trace function which
  // returns a value, currently we donot support return in our trace.
  if (!Function.getReturnType()->isVoidTy()) {
    Accelerable = false;
  }
  if (!Accelerable) {
    return false;
  }

  DEBUG(llvm::errs() << "Found accelerable function: " << FunctionName << '\n');

  // Change the function body to call ioctl
  // to replay the trace.
  // First simply remove all BBs.
  Function.deleteBody();

  // Add another entry block with a simple ioctl call.
  auto NewBB =
      llvm::BasicBlock::Create(this->Module->getContext(), "entry", &Function);
  llvm::IRBuilder<> Builder(NewBB);

  std::vector<llvm::Value*> ReplayArgs;
  for (auto ArgIter = Function.arg_begin(), ArgEnd = Function.arg_end();
       ArgIter != ArgEnd; ArgIter++) {
    if (ArgIter->getType()->getTypeID() != llvm::Type::TypeID::PointerTyID) {
      continue;
    }
    // Map all pointer
    auto ArgName = ArgIter->getName();
    auto ArgNameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, ArgName);
    auto CastAddrValue = Builder.CreateCast(
        llvm::Instruction::CastOps::BitCast, &*ArgIter,
        llvm::Type::getInt8PtrTy(this->Module->getContext()));
    ReplayArgs.push_back(ArgNameValue);
    ReplayArgs.push_back(CastAddrValue);
  }

  // Insert replay call.
  auto TraceNameValue = getOrCreateStringLiteral(
      this->GlobalStrings, this->Module, Function.getName());
  auto NumMapsValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->Module->getContext()),
      ReplayArgs.size() / 2, false);
  ReplayArgs.insert(ReplayArgs.begin(), NumMapsValue);
  ReplayArgs.insert(ReplayArgs.begin(), TraceNameValue);
  Builder.CreateCall(this->ReplayFunc, ReplayArgs);

  // Remember to return void.
  Builder.CreateRet(nullptr);

  // We always modify the function.
  return true;
}

// The default transformation is just an identical transformation.
// Other pass can override this function to perform their own transformation.
void ReplayTrace::TransformTrace() {
  assert(this->Trace != nullptr && "Must have a trace to be transformed.");
  // Simply generate the output data graph for gem5 to use.
  std::ofstream OutTrace(this->OutTraceName);
  assert(OutTrace.is_open() && "Failed to open output trace file.");
  for (DynamicId Id = 0, NumDynamicInsts = this->Trace->DyanmicInstsMap.size();
       Id != NumDynamicInsts; ++Id) {
    DynamicInstruction* DynamicInst = this->Trace->DyanmicInstsMap.at(Id);
    this->formatInstruction(DynamicInst, OutTrace);
    OutTrace << '\n';
  }
  OutTrace.close();
}

// Insert all the print function declaration into the module.
void ReplayTrace::registerFunction(llvm::Module& Module) {
  auto& Context = Module.getContext();
  auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
  auto VoidTy = llvm::Type::getVoidTy(Context);
  auto Int64Ty = llvm::Type::getInt64Ty(Context);

  std::vector<llvm::Type*> ReplayArgs{
      Int8PtrTy,
      Int64Ty,
  };
  auto ReplayTy = llvm::FunctionType::get(VoidTy, ReplayArgs, true);
  this->ReplayFunc = Module.getOrInsertFunction("replay", ReplayTy);
}

//************************************************************************//
// Helper function to generate the trace for gem5.
//************************************************************************//
void ReplayTrace::formatInstruction(DynamicInstruction* DynamicInst,
                                    std::ofstream& Out) {  // The op_code field.
  this->formatOpCode(DynamicInst->StaticInstruction, Out);
  Out << '|';
  // The dependence field.
  this->formatDeps(DynamicInst, Out);
  Out << '|';
  // Other fields for other isnsts.
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->StaticInstruction)) {
    // base|offset|trace_vaddr|size|
    DynamicValue* LoadedAddr = DynamicInst->DynamicOperands[0];
    uint64_t LoadedSize = this->Trace->DataLayout->getTypeStoreSize(
        LoadStaticInstruction->getPointerOperandType()
            ->getPointerElementType());
    Out << LoadedAddr->MemBase << '|' << LoadedAddr->MemOffset << '|'
        << LoadedAddr->Value << '|' << LoadedSize << '|';
    return;
  }

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(DynamicInst->StaticInstruction)) {
    // base|offset|trace_vaddr|size|type_id|type_name|value|
    DynamicValue* StoredAddr = DynamicInst->DynamicOperands[1];
    llvm::Type* StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredSize = this->Trace->DataLayout->getTypeStoreSize(StoredType);
    std::string TypeName;
    {
      llvm::raw_string_ostream Stream(TypeName);
      StoredType->print(Stream);
    }
    Out << StoredAddr->MemBase << '|' << StoredAddr->MemOffset << '|'
        << StoredAddr->Value << '|' << StoredSize << '|'
        << StoredType->getTypeID() << '|' << TypeName << '|'
        << DynamicInst->DynamicOperands[0]->Value << '|';
    return;
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(DynamicInst->StaticInstruction)) {
    // base|trace_vaddr|size|
    uint64_t AllocatedSize = this->Trace->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    Out << DynamicInst->DynamicResult->MemBase << '|'
        << DynamicInst->DynamicResult->Value << '|' << AllocatedSize << '|';
    return;
  }
}

void ReplayTrace::formatDeps(DynamicInstruction* DynamicInst,
                             std::ofstream& Out) {
  {
    auto DepIter = this->Trace->RegDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->RegDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->MemDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->MemDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->MemDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->MemDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
}

void ReplayTrace::formatOpCode(llvm::Instruction* StaticInstruction,
                               std::ofstream& Out) {
  if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(StaticInstruction)) {
    llvm::Function* Callee = CallInst->getCalledFunction();
    if (Callee->isIntrinsic()) {
      // For instrinsic, use "call_intrinsic" for now.
      Out << "call_intrinsic";
    } else if (Callee->getName() == "sin") {
      Out << "sin";
    } else if (Callee->getName() == "cos") {
      Out << "cos";
    } else {
      Out << "call";
    }
    return;
  }
  // For other inst, just use the original op code.
  Out << StaticInstruction->getOpcodeName();
}
#undef DEBUG_TYPE

char ReplayTrace::ID = 0;
static llvm::RegisterPass<ReplayTrace> R("replay", "replay the llvm trace",
                                         false, false);