#include "DynamicTrace.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include "LocateAccelerableFunctions.h"

#include <map>
#include <set>

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

class ReplayTrace : public llvm::FunctionPass {
 public:
  static char ID;
  ReplayTrace() : llvm::FunctionPass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addPreserved<llvm::LoopInfoWrapperPass>();
    Info.addRequiredID(llvm::InstructionNamerID);
    Info.addPreservedID(llvm::InstructionNamerID);
  }

  bool doInitialization(llvm::Module& Module) override {
    this->Module = &Module;

    assert(TraceFileName.getNumOccurrences() == 1 &&
           "Please specify the trace file.");
    this->Trace = new DynamicTrace(TraceFileName, this->Module);

    DEBUG(llvm::errs() << "Parsed # dynamic insts: "
                       << this->Trace->DyanmicInstsMap.size() << '\n');

    Workload = "fft";
    DEBUG(llvm::errs() << "Initialize ReplaceTrace with workload: " << Workload
                       << '\n');

    // Register the external ioctl function.
    registerFunction(Module);

    return true;
  }

  bool runOnFunction(llvm::Function& Function) override {
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

    DEBUG(llvm::errs() << "Found workload: " << FunctionName << '\n');

    // Change the function body to call ioctl
    // to replay the trace.
    // First simply remove all BBs.
    Function.deleteBody();

    // Add another entry block with a simple ioctl call.
    auto NewBB = llvm::BasicBlock::Create(this->Module->getContext(), "entry",
                                          &Function);
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

 private:
  DynamicTrace* Trace;

  std::string Workload;
  llvm::Module* Module;

  llvm::Value* ReplayFunc;

  std::map<std::string, llvm::Constant*> GlobalStrings;

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module& Module) {
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
};
}  // namespace
#undef DEBUG_TYPE

char ReplayTrace::ID = 0;
static llvm::RegisterPass<ReplayTrace> R("replay", "replay the llvm trace",
                                         false, false);