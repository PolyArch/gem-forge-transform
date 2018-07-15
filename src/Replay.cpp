#include "Replay.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include <fstream>

llvm::cl::opt<DataGraph::DataGraphDetailLv> DataGraphDetailLevel(
    "datagraph-detail",
    llvm::cl::desc("Choose how detail the datagraph should be:"),
    llvm::cl::values(
        clEnumValN(DataGraph::DataGraphDetailLv::SIMPLE, "simple",
                   "Only instruction, without dynamic values"),
        clEnumValN(DataGraph::DataGraphDetailLv::STANDALONE, "standalone",
                   "With dynamic values, without memory base/offset"),
        clEnumValN(DataGraph::DataGraphDetailLv::INTEGRATED, "integrated",
                   "All")));

#define DEBUG_TYPE "ReplayPass"
namespace {

llvm::Constant *
getOrCreateStringLiteral(std::map<std::string, llvm::Constant *> &GlobalStrings,
                         llvm::Module *Module, const std::string &Str) {
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
    std::vector<llvm::Constant *> Indexes;
    Indexes.push_back(ConstantZero);
    Indexes.push_back(ConstantZero);
    GlobalStrings[Str] = llvm::ConstantExpr::getGetElementPtr(
        ArrayTy, GlobalVariableStr, Indexes);
  }
  return GlobalStrings.at(Str);
}
} // namespace

// Fake push instruction.
class PushInstruction : public DynamicInstruction {
public:
  // Let's not worry about the align for now.
  PushInstruction(const DynamicValue &_Value) : Value(_Value) {}
  std::string getOpName() const override { return "push"; }
  void formatCustomizedFields(llvm::raw_ostream &Out,
                              DataGraph *Trace) const override {}
  DynamicValue Value;
};

class CallExternalInstruction : public DynamicInstruction {
public:
  // Let's not worry about the align for now.
  CallExternalInstruction(const std::string &_Symbol) : Symbol(_Symbol) {}
  std::string getOpName() const override { return "call-external"; }
  void formatCustomizedFields(llvm::raw_ostream &Out,
                              DataGraph *Trace) const override {}
  std::string Symbol;
};

class RetExternalInstruction : public DynamicInstruction {
public:
  // Let's not worry about the align for now.
  RetExternalInstruction(const DynamicValue &_Result) : Result(_Result) {}
  std::string getOpName() const override { return "ret-external"; }
  void formatCustomizedFields(llvm::raw_ostream &Out,
                              DataGraph *Trace) const override {}
  DynamicValue Result;
};

ReplayTrace::ReplayTrace(char _ID)
    : llvm::ModulePass(_ID), Trace(nullptr),
      OutTraceName("llvm_trace_gem5.txt"), Serializer(nullptr) {}

ReplayTrace::~ReplayTrace() {}

void ReplayTrace::getAnalysisUsage(llvm::AnalysisUsage &Info) const {
  Info.addRequired<LocateAccelerableFunctions>();
  Info.addPreserved<LocateAccelerableFunctions>();
}

bool ReplayTrace::runOnModule(llvm::Module &Module) {
  this->initialize(Module);

  // Do the trasformation only once.
  this->transform();

  bool Modified = false;
  if (this->Trace->DetailLevel == DataGraph::DataGraphDetailLv::INTEGRATED) {
    for (auto FuncIter = Module.begin(), FuncEnd = Module.end();
         FuncIter != FuncEnd; ++FuncIter) {
      llvm::Function &Function = *FuncIter;
      Modified |= this->processFunction(Function);
    }
  }

  this->finalize(Module);

  return Modified;
}

bool ReplayTrace::initialize(llvm::Module &Module) {
  DEBUG(llvm::errs() << "ReplayTrace::doInitialization.\n");
  this->Module = &Module;

  // Register the external ioctl function.
  registerFunction(Module);

  // For replay, the datagraph should be at least standalone mode.

  // If user specify the detail level, we only allow
  // it upgrades.

  auto DetailLevel = DataGraph::DataGraphDetailLv::STANDALONE;
  if (DataGraphDetailLevel.getNumOccurrences() == 1) {
    assert(DataGraphDetailLevel >= DataGraph::DataGraphDetailLv::STANDALONE &&
           "User specified detail level is lower than standalone.");
    DetailLevel = DataGraphDetailLevel;
  }

  DEBUG(llvm::errs() << "Initialize the datagraph with detail level "
                     << DetailLevel << ".\n");
  this->Trace = new DataGraph(this->Module, DetailLevel);
  this->Serializer = new TDGSerializer(this->OutTraceName);

  return true;
}

bool ReplayTrace::finalize(llvm::Module &Module) {
  DEBUG(llvm::errs() << "Releasing serializer at " << this->Serializer << '\n');
  delete this->Serializer;
  this->Serializer = nullptr;
  DEBUG(llvm::errs() << "Releasing datagraph at " << this->Trace << '\n');
  delete this->Trace;
  this->Trace = nullptr;
  return true;
}

bool ReplayTrace::processFunction(llvm::Function &Function) {

  auto FunctionName = Function.getName().str();
  DEBUG(llvm::errs() << "FunctionName: " << FunctionName << '\n');

  if (Function.isIntrinsic()) {
    return false;
  }

  if (Function.isDeclaration()) {
    return false;
  }

  if (Function.getName() == "replay") {
    return false;
  }

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

  std::vector<llvm::Value *> ReplayArgs;
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

class FakeDynamicInstruction : public DynamicInstruction {
public:
  FakeDynamicInstruction(const std::string &_OpName,
                         DynamicValue *_DynamicResult,
                         std::vector<DynamicValue *> _DynamicOperands)
      : DynamicInstruction(), OpName(_OpName) {
    this->DynamicResult = _DynamicResult;
    this->DynamicOperands = std::move(_DynamicOperands);
  }
  std::string getOpName() const override { return this->OpName; }
  std::string OpName;
};

static DynamicInstruction *
createFakeDynamicLoad(llvm::Instruction *StaticInst) {
  assert(StaticInst != nullptr && "Null static instruction.");
  DynamicValue *Result = new DynamicValue("0");
  DynamicValue *Operand = new DynamicValue("0");
  Operand->MemBase = "$sp";
  Operand->MemOffset = 0;
  auto Operands = std::vector<DynamicValue *>{Operand};
  return new LLVMDynamicInstruction(StaticInst, Result, std::move(Operands));
}

static DynamicInstruction *
createFakeDynamicStore(llvm::Instruction *StaticInst) {
  DynamicValue *StoreValue = new DynamicValue("0");
  DynamicValue *Operand = new DynamicValue("0");
  Operand->MemBase = "$sp";
  Operand->MemOffset = 0;
  auto Operands = std::vector<DynamicValue *>{StoreValue, Operand};
  return new LLVMDynamicInstruction(StaticInst, nullptr, std::move(Operands));
}

void ReplayTrace::fakeRegisterAllocation() {
  // assert(this->Trace != nullptr &&
  //        "Must have a trace to be transformed to fake register allocation.");

  // const size_t NUM_PHYS_REGS = 8;

  // DynamicInstruction* Iter = this->Trace->DynamicInstructionListTail;
  // std::unordered_set<DynamicInstruction*> LiveDynamicInsts;
  // std::list<DynamicInstruction*> InsertedDynamicLoads;

  // uint64_t FakeSpillCount = 0;
  // while (Iter != nullptr) {
  //   // First if Iter is in the live set, remove it.
  //   DynamicInstruction* IterPrev = Iter->Prev;
  //   InsertedDynamicLoads.clear();
  //   if (LiveDynamicInsts.find(Iter) != LiveDynamicInsts.end()) {
  //     LiveDynamicInsts.erase(Iter);
  //   }
  //   // For all the register depedence, add them into the live set.
  //   if (this->Trace->RegDeps.find(Iter) != this->Trace->RegDeps.end()) {
  //     auto& DepInsts = this->Trace->RegDeps.at(Iter);
  //     for (auto& DepInst : DepInsts) {
  //       if (LiveDynamicInsts.find(DepInst) != LiveDynamicInsts.end()) {
  //         // This value is already live.
  //         continue;
  //       }
  //       if (LiveDynamicInsts.size() + 1 > NUM_PHYS_REGS) {
  //         // Spill a random value.
  //         DynamicInstruction* SpillInst = nullptr;
  //         for (auto& LiveInst : LiveDynamicInsts) {
  //           // Do not accidently spill out something we are using.
  //           if (DepInsts.find(LiveInst) != DepInsts.end()) {
  //             continue;
  //           }
  //           SpillInst = LiveInst;
  //           break;
  //         }
  //         // Remove the spill inst from the live set.
  //         LiveDynamicInsts.erase(SpillInst);
  //         FakeSpillCount++;
  //         // Simply insert a fake load/store pair of 64 bits to stack.
  //         auto FakeDynamicLoad =
  //         createFakeDynamicLoad(this->FakeRegisterFill); auto
  //         FakeDynamicStore =
  //             createFakeDynamicStore(this->FakeRegisterSpill);
  //         // Add a simple reg dependence between load and iter.
  //         // store
  //         // load
  //         // iter
  //         // Insert into the list.
  //         insertDynamicInst(FakeDynamicStore, Iter->Prev, Iter,
  //                           &(this->Trace->DynamicInstructionListHead),
  //                           &(this->Trace->DynamicInstructionListTail));
  //         insertDynamicInst(FakeDynamicLoad, Iter->Prev, Iter,
  //                           &(this->Trace->DynamicInstructionListHead),
  //                           &(this->Trace->DynamicInstructionListTail));
  //         InsertedDynamicLoads.emplace_back(FakeDynamicLoad);
  //       }
  //       // Add the dep inst into the live set.
  //       LiveDynamicInsts.insert(DepInst);
  //     }
  //     // Insert dependence on the load.
  //     for (auto& InsertedDynamicLoad : InsertedDynamicLoads) {
  //       this->Trace->RegDeps.at(Iter).insert(InsertedDynamicLoad);
  //     }
  //   }
  //   Iter = IterPrev;
  // }
  // DEBUG(llvm::errs() << "Inserted fake spills " << FakeSpillCount << '\n');
}

static DynamicInstruction *createFakeDynamicInst(const std::string &OpName) {
  auto Operands = std::vector<DynamicValue *>();
  return new FakeDynamicInstruction(OpName, nullptr, std::move(Operands));
}

void ReplayTrace::fakeFixRegisterDeps() {
  // In gem5, mul/div is fixed to write to rax.
  // We fake this dependence.
  DynamicInstruction *PrevMulDivInst = nullptr;
  for (auto Iter = this->Trace->DynamicInstructionList.begin(),
            End = this->Trace->DynamicInstructionList.end();
       Iter != End; ++Iter) {
    DynamicInstruction *DynamicInst = *Iter;
    auto StaticInstruction = DynamicInst->getStaticInstruction();
    switch (StaticInstruction->getOpcode()) {
    // case llvm::Instruction::FDiv:
    case llvm::Instruction::FMul:
    case llvm::Instruction::Mul:
    case llvm::Instruction::UDiv:
    case llvm::Instruction::SDiv:
    case llvm::Instruction::URem:
    case llvm::Instruction::SRem: {
      // Add the fake register dependence.
      if (PrevMulDivInst != nullptr) {
        if (this->Trace->RegDeps.find(DynamicInst->Id) ==
            this->Trace->RegDeps.end()) {
          this->Trace->RegDeps.emplace(
              DynamicInst->Id,
              std::unordered_set<DynamicInstruction::DynamicId>());
        }
        this->Trace->RegDeps.at(DynamicInst->Id).insert(PrevMulDivInst->Id);
      }
      PrevMulDivInst = DynamicInst;
      break;
    }
    default:
      break;
    }
  }
}

void ReplayTrace::fakeMicroOps() {
  for (auto Iter = this->Trace->DynamicInstructionList.begin(),
            End = this->Trace->DynamicInstructionList.end();
       Iter != End; ++Iter) {
    DynamicInstruction *DynamicInst = *Iter;

    auto StaticInstruction = DynamicInst->getStaticInstruction();

    if (StaticInstruction != nullptr) {
      // This is an llvm instruction.
      // Ignore all the phi node and switch.
      if (StaticInstruction->getOpcode() != llvm::Instruction::PHI &&
          StaticInstruction->getOpcode() != llvm::Instruction::Switch) {
        // Fake micro ops pending to be inserted.
        // If 1, means that the fake micro op should be inserted
        // into the dependent path.
        std::vector<std::pair<DynamicInstruction *, int>> FakeNumMicroOps;
        for (unsigned int Idx = 0,
                          NumOperands = StaticInstruction->getNumOperands();
             Idx != NumOperands; ++Idx) {
          if (llvm::isa<llvm::Constant>(StaticInstruction->getOperand(Idx))) {
            // This is an immediate number.
            // No dependency for loading an immediate.
            FakeNumMicroOps.emplace_back(createFakeDynamicInst("limm"), 0);
          }
        }

        // DEBUG(llvm::errs() << StaticInstruction->getName() << ' '
        //                    << StaticInstruction->getOpcodeName() << '\n');

        switch (StaticInstruction->getOpcode()) {
        case llvm::Instruction::FMul: {
          auto Operand = StaticInstruction->getOperand(0);
          if (Operand->getType()->isVectorTy()) {
            FakeNumMicroOps.emplace_back(createFakeDynamicInst("fmul"), 1);
          }
          break;
        }
        case llvm::Instruction::Mul: {
          // Add mulel and muleh op.
          // Normally this should be
          // mulix
          // mulel
          // mules
          // But now we fake it as:
          // limm
          // limm
          // mul
          FakeNumMicroOps.emplace_back(createFakeDynamicInst("mul"), 1);
          break;
        }
        case llvm::Instruction::Switch:
        case llvm::Instruction::Br: {
          // rdip
          // wrip
          FakeNumMicroOps.emplace_back(createFakeDynamicInst("limm"), 0);
          FakeNumMicroOps.emplace_back(createFakeDynamicInst("limm"), 0);
          break;
        }
        default:
          break;
        }

        std::vector<DynamicInstruction *> Fakes;
        Fakes.reserve(FakeNumMicroOps.size());
        for (const auto &FakeOpEntry : FakeNumMicroOps) {
          auto FakeInstruction = FakeOpEntry.first;
          if (FakeOpEntry.second == 1) {
            // If FakeDep, make the micro op dependent on inst's dependence.
            if (this->Trace->RegDeps.find(DynamicInst->Id) !=
                this->Trace->RegDeps.end()) {
              this->Trace->RegDeps.emplace(
                  FakeInstruction->Id,
                  this->Trace->RegDeps.at(DynamicInst->Id));
            }
            if (this->Trace->CtrDeps.find(DynamicInst->Id) !=
                this->Trace->CtrDeps.end()) {
              this->Trace->CtrDeps.emplace(
                  FakeInstruction->Id,
                  this->Trace->CtrDeps.at(DynamicInst->Id));
            }
          }
          Fakes.push_back(FakeInstruction);
        }

        // Insert all the fake operations.
        this->Trace->DynamicInstructionList.insert(Iter, Fakes.begin(),
                                                   Fakes.end());

        for (auto FakeOp : Fakes) {
          // Fix the dependence.
          // Make the inst dependent on the micro op.
          if (this->Trace->RegDeps.find(DynamicInst->Id) ==
              this->Trace->RegDeps.end()) {
            this->Trace->RegDeps.emplace(
                DynamicInst->Id,
                std::unordered_set<DynamicInstruction::DynamicId>());
          }
          this->Trace->RegDeps.at(DynamicInst->Id).insert(FakeOp->Id);
        }
      }
    }
  }
}

// This function will make external call possible in the datagraph.
// It only support the C calling convention and set up the stack by
// push
// external-call
// external-call-clean
void ReplayTrace::fakeExternalCall(DataGraph::DynamicInstIter InstIter) {
  auto DynamicInst = *InstIter;
  auto StaticInst = DynamicInst->getStaticInstruction();
  if (StaticInst == nullptr) {
    // This is not a llvm instruction.
    return;
  }
  if (auto StaticCall = llvm::dyn_cast<llvm::CallInst>(StaticInst)) {
    auto Callee = StaticCall->getCalledFunction();
    if (Callee == nullptr) {
      // This is indirect call, we assume it's traced.
      // (This is really bad, it all comes from that we cannot determine the
      // dynamic dispatch.)
      // TODO: Improve this?
      return;
    }
    if (!Callee->isDeclaration()) {
      // We have definition of this function, it will be traced.
      return;
    }
    // This is an untraced call.
    auto CallingConvention = StaticCall->getCallingConv();
    assert(CallingConvention == llvm::CallingConv::C &&
           "We only support C calling convention.");
    // Create the push instructions.
    std::vector<DynamicInstruction *> Pushes;
    Pushes.reserve(StaticCall->getNumArgOperands());
    for (unsigned ArgIdx = 0, NumArgs = StaticCall->getNumArgOperands();
         ArgIdx != NumArgs; ++ArgIdx) {
      Pushes.push_back(
          new PushInstruction(*(DynamicInst->DynamicOperands[ArgIdx + 1])));
    }
    // The push instruction will implicitly wait until the previous one is
    // committed, so no need to insert dependence edges.
    // Create the call-external instruction and call-external-clean.
    // auto CallExternal = new
  }
}

// The default transformation is just an identical transformation.
// Other pass can override this function to perform their own transformation.
void ReplayTrace::transform() {
  assert(this->Trace != nullptr && "Must have a trace to be transformed.");

  // Simple sliding window method.
  uint64_t Count = 0;
  bool Ended = false;
  while (true) {
    if (!Ended) {
      auto NewDynamicInst = this->Trace->loadOneDynamicInst();
      if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
        Count++;
      } else {
        Ended = true;
      }
    }

    if (Count == 0 && Ended) {
      // We are done.
      break;
    }

    // Generate the trace for gem5.
    // Maintain the window size of 10000?
    const uint64_t Window = 10000;
    if (Count > Window || Ended) {
      auto Iter = this->Trace->DynamicInstructionList.begin();
      this->Serializer->serialize(*Iter, this->Trace);
      Count--;
      this->Trace->commitOneDynamicInst();
    }
  }
}

// Insert all the print function declaration into the module.
void ReplayTrace::registerFunction(llvm::Module &Module) {
  auto &Context = Module.getContext();
  auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
  auto VoidTy = llvm::Type::getVoidTy(Context);
  auto Int64Ty = llvm::Type::getInt64Ty(Context);

  std::vector<llvm::Type *> ReplayArgs{
      Int8PtrTy,
      Int64Ty,
  };
  auto ReplayTy = llvm::FunctionType::get(VoidTy, ReplayArgs, true);
  this->ReplayFunc = Module.getOrInsertFunction("replay", ReplayTy);

  auto FakeRegisterAllocationTy = llvm::FunctionType::get(VoidTy, false);
  auto FakeRegisterAllocationFunc = llvm::Function::Create(
      FakeRegisterAllocationTy, llvm::Function::InternalLinkage,
      "LLVM_TDG_FAKE_REGISTER_ALLOCATION", &Module);

  auto FakeRegisterAllocationBB = llvm::BasicBlock::Create(
      Module.getContext(), "entry", FakeRegisterAllocationFunc);
  llvm::IRBuilder<> Builder(FakeRegisterAllocationBB);

  auto FakeRegisterAllocationAddr = llvm::ConstantPointerNull::get(
      llvm::IntegerType::getInt64PtrTy(Module.getContext()));
  auto FakeRegisterAllocationValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(Module.getContext()), 0);

  this->FakeRegisterFill =
      Builder.CreateLoad(FakeRegisterAllocationAddr, "FakeRegisterFill64");
  this->FakeRegisterSpill = Builder.CreateStore(FakeRegisterAllocationValue,
                                                FakeRegisterAllocationAddr);
  DEBUG(llvm::errs() << this->FakeRegisterFill->getName() << '\n');
  Builder.CreateRetVoid();
}
#undef DEBUG_TYPE

char ReplayTrace::ID = 0;
static llvm::RegisterPass<ReplayTrace> R("replay", "replay the llvm trace",
                                         false, false);