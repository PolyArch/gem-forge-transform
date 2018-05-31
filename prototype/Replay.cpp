#include "Replay.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include <fstream>

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

ReplayTrace::ReplayTrace(char _ID)
    : llvm::FunctionPass(_ID),
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

  // Register the external ioctl function.
  registerFunction(Module);

  this->Trace = new DataGraph(this->Module);

  // DEBUG(llvm::errs() << "Parsed # memory dependences: "
  //                    << this->Trace->NumMemDependences << '\n');

  // Generate the transformation of the trace.
  this->TransformTrace();

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

class FakeDynamicInstruction : public DynamicInstruction {
 public:
  FakeDynamicInstruction(const std::string& _OpName,
                         DynamicValue* _DynamicResult,
                         std::vector<DynamicValue*> _DynamicOperands,
                         DynamicInstruction* _Prev, DynamicInstruction* _Next)
      : DynamicInstruction(), OpName(_OpName) {
    this->DynamicResult = _DynamicResult;
    this->DynamicOperands = std::move(_DynamicOperands);
    this->Prev = _Prev;
    this->Next = _Next;
  }
  const std::string& getOpName() override { return this->OpName; }
  std::string OpName;
};

static DynamicInstruction* createFakeDynamicLoad(
    llvm::Instruction* StaticInst) {
  assert(StaticInst != nullptr && "Null static instruction.");
  DynamicValue* Result = new DynamicValue("0");
  DynamicValue* Operand = new DynamicValue("0");
  Operand->MemBase = "$sp";
  Operand->MemOffset = 0;
  auto Operands = std::vector<DynamicValue*>{Operand};
  return new LLVMDynamicInstruction(StaticInst, Result, std::move(Operands),
                                    nullptr, nullptr);
}

static DynamicInstruction* createFakeDynamicStore(
    llvm::Instruction* StaticInst) {
  DynamicValue* StoreValue = new DynamicValue("0");
  DynamicValue* Operand = new DynamicValue("0");
  Operand->MemBase = "$sp";
  Operand->MemOffset = 0;
  auto Operands = std::vector<DynamicValue*>{StoreValue, Operand};
  return new LLVMDynamicInstruction(StaticInst, nullptr, std::move(Operands),
                                    nullptr, nullptr);
}

static void insertDynamicInst(DynamicInstruction* NewInst,
                              DynamicInstruction* InsertAfter,
                              DynamicInstruction* InsertBefore,
                              DynamicInstruction** Head,
                              DynamicInstruction** Tail) {
  NewInst->Prev = InsertAfter;
  NewInst->Next = InsertBefore;
  if (InsertAfter != nullptr) {
    assert(InsertAfter->Next == InsertBefore &&
           "Invalid next dynamic instruction.");
    InsertAfter->Next = NewInst;
  } else {
    *Head = NewInst;
  }
  if (InsertBefore != nullptr) {
    assert(InsertBefore->Prev == InsertAfter &&
           "Invalid prev dynamic instruction.");
    InsertBefore->Prev = NewInst;
  } else {
    *Tail = NewInst;
  }
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

static DynamicInstruction* createFakeDynamicInst(const std::string& OpName) {
  auto Operands = std::vector<DynamicValue*>();
  return new FakeDynamicInstruction(OpName, nullptr, std::move(Operands),
                                    nullptr, nullptr);
}

void ReplayTrace::fakeFixRegisterDeps() {
  DynamicInstruction* Iter = this->Trace->DynamicInstructionListHead;
  // In gem5, mul/div is fixed to write to rax.
  // We fake this dependence.
  DynamicInstruction* PrevMulDivInst = nullptr;
  while (Iter != nullptr) {
    if (Iter->getStaticInstruction() != nullptr) {
      auto StaticInstruction = Iter->getStaticInstruction();
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
            if (this->Trace->RegDeps.find(Iter->Id) ==
                this->Trace->RegDeps.end()) {
              this->Trace->RegDeps.emplace(
                  Iter->Id,
                  std::unordered_set<DynamicInstruction::DynamicId>());
            }
            this->Trace->RegDeps.at(Iter->Id).insert(PrevMulDivInst->Id);
          }
          PrevMulDivInst = Iter;
          break;
        }
        default:
          break;
      }
      Iter = Iter->Next;
    }
  }
}

void ReplayTrace::fakeMicroOps() {
  DynamicInstruction* Iter = this->Trace->DynamicInstructionListHead;
  while (Iter != nullptr) {
    ;
    auto IterPrev = Iter->Prev;
    auto IterNext = Iter->Next;

    if (Iter->getStaticInstruction() != nullptr) {
      // This is an llvm instruction.
      auto StaticInstruction = Iter->getStaticInstruction();

      // Ignore all the phi node and switch.
      if (StaticInstruction->getOpcode() != llvm::Instruction::PHI &&
          StaticInstruction->getOpcode() != llvm::Instruction::Switch) {
        // Fake micro ops pending to be inserted.
        // If 1, means that the fake micro op should be inserted
        // into the dependent path.
        std::vector<std::pair<DynamicInstruction*, int>> FakeNumMicroOps;
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

        std::vector<DynamicInstruction*> Fakes;
        Fakes.reserve(FakeNumMicroOps.size());
        for (const auto& FakeOpEntry : FakeNumMicroOps) {
          auto FakeInstruction = FakeOpEntry.first;
          if (FakeOpEntry.second == 1) {
            // If FakeDep, make the micro op dependent on inst's dependence.
            if (this->Trace->RegDeps.find(Iter->Id) !=
                this->Trace->RegDeps.end()) {
              this->Trace->RegDeps.emplace(FakeInstruction->Id,
                                           this->Trace->RegDeps.at(Iter->Id));
            }
            if (this->Trace->CtrDeps.find(Iter->Id) !=
                this->Trace->CtrDeps.end()) {
              this->Trace->CtrDeps.emplace(FakeInstruction->Id,
                                           this->Trace->CtrDeps.at(Iter->Id));
            }
          }
          Fakes.push_back(FakeInstruction);
        }

        for (auto FakeOp : Fakes) {
          insertDynamicInst(FakeOp, IterPrev, Iter,
                            &(this->Trace->DynamicInstructionListHead),
                            &(this->Trace->DynamicInstructionListTail));
          // Fix the dependence.
          // Make the inst dependent on the micro op.
          if (this->Trace->RegDeps.find(Iter->Id) ==
              this->Trace->RegDeps.end()) {
            this->Trace->RegDeps.emplace(
                Iter->Id, std::unordered_set<DynamicInstruction::DynamicId>());
          }
          this->Trace->RegDeps.at(Iter->Id).insert(FakeOp->Id);
          IterPrev = FakeOp;
        }
      }
    }

    Iter = IterNext;
  }
}

// The default transformation is just an identical transformation.
// Other pass can override this function to perform their own transformation.
void ReplayTrace::TransformTrace() {
  assert(this->Trace != nullptr && "Must have a trace to be transformed.");

  std::ofstream OutTrace(this->OutTraceName);
  assert(OutTrace.is_open() && "Failed to open output trace file.");

  // Simple sliding window method.
  uint64_t Count = 0;
  bool Ended = false;
  while (true) {
    if (!Ended) {
      auto NewDynamicInst = this->Trace->loadOneDynamicInst();
      if (NewDynamicInst != nullptr) {
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
      this->formatInstruction(this->Trace->DynamicInstructionListHead,
                              OutTrace);
      OutTrace << '\n';
      Count--;
      this->Trace->commitOneDynamicInst();
    }
  }

  OutTrace.close();

  // First we fake the register allocation, and then the micro ops.
  // this->fakeRegisterAllocation();
  // this->fakeFixRegisterDeps();
  // DEBUG(llvm::errs() << "Start fake microops what happend.\n");
  // this->fakeMicroOps();
  // DEBUG(llvm::errs() << "Start outputing.\n");
  // Simply generate the output data graph for gem5 to use.
  // std::ofstream OutTrace(this->OutTraceName);
  // assert(OutTrace.is_open() && "Failed to open output trace file.");
  // DynamicInstruction* Iter = this->Trace->DynamicInstructionListHead;
  // while (Iter != nullptr) {
  //   this->formatInstruction(Iter, OutTrace);
  //   OutTrace << '\n';
  //   Iter = Iter->Next;
  // }
  // OutTrace.close();
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

//************************************************************************//
// Helper function to generate the trace for gem5.
//************************************************************************//
void ReplayTrace::formatInstruction(DynamicInstruction* DynamicInst,
                                    std::ofstream& Out) {  // The op_code field.
  this->formatOpCode(DynamicInst, Out);
  Out << '|';
  // The faked number of micro ops.
  uint32_t FakeNumMicroOps = 1;
  Out << DynamicInst->Id << '|';
  // The dependence field.
  this->formatDeps(DynamicInst, Out);
  Out << '|';
  // Other fields for other insts.
  // If this is a self defined inst.
  if (DynamicInst->getStaticInstruction() == nullptr) {
    // Simply return.
    return;
  }

  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->getStaticInstruction())) {
    // base|offset|trace_vaddr|size|
    DynamicValue* LoadedAddr = DynamicInst->DynamicOperands[0];
    uint64_t LoadedSize = this->Trace->DataLayout->getTypeStoreSize(
        LoadStaticInstruction->getPointerOperandType()
            ->getPointerElementType());
    Out << LoadedAddr->MemBase << '|' << LoadedAddr->MemOffset << '|'
        << LoadedAddr->Value << '|' << LoadedSize << '|';
    // This load inst will produce some new base for future memory access.
    Out << DynamicInst->DynamicResult->MemBase << '|';
    return;
  }

  if (auto StoreStaticInstruction = llvm::dyn_cast<llvm::StoreInst>(
          DynamicInst->getStaticInstruction())) {
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

  if (auto AllocaStaticInstruction = llvm::dyn_cast<llvm::AllocaInst>(
          DynamicInst->getStaticInstruction())) {
    // base|trace_vaddr|size|
    uint64_t AllocatedSize = this->Trace->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    Out << DynamicInst->DynamicResult->MemBase << '|'
        << DynamicInst->DynamicResult->Value << '|' << AllocatedSize << '|';
    return;
  }

  // For conditional branching, we also log the target basic block.
  if (auto BranchStaticInstruction = llvm::dyn_cast<llvm::BranchInst>(
          DynamicInst->getStaticInstruction())) {
    if (BranchStaticInstruction->isConditional()) {
      DynamicInstruction* NextLLVMDynamicInst = DynamicInst->Next;
      while (NextLLVMDynamicInst != nullptr) {
        // Check if this is an LLVM inst, i.e. not our inserted.
        if (NextLLVMDynamicInst->getStaticInstruction() != nullptr) {
          break;
        }
        NextLLVMDynamicInst = NextLLVMDynamicInst->Next;
      }
      // Log the static instruction's address and target branch name.
      // Use the memory address as the identifier for static instruction
      // is very hacky, but it does maintain unique.
      std::string NextBBName =
          NextLLVMDynamicInst->getStaticInstruction()->getParent()->getName();
      Out << BranchStaticInstruction << '|' << NextBBName << '|';
    }
    return;
  }

  // For switch, do the same.
  // TODO: Reuse code with branch case.
  if (auto SwitchStaticInstruction = llvm::dyn_cast<llvm::SwitchInst>(
          DynamicInst->getStaticInstruction())) {
    DynamicInstruction* NextLLVMDynamicInst = DynamicInst->Next;
    while (NextLLVMDynamicInst != nullptr) {
      // Check if this is an LLVM inst, i.e. not our inserted.
      if (NextLLVMDynamicInst->getStaticInstruction() != nullptr) {
        break;
      }
      NextLLVMDynamicInst = NextLLVMDynamicInst->Next;
    }
    // Log the static instruction's address and target branch name.
    // Use the memory address as the identifier for static instruction
    // is very hacky, but it does maintain unique.
    std::string NextBBName =
        NextLLVMDynamicInst->getStaticInstruction()->getParent()->getName();
    Out << SwitchStaticInstruction << '|' << NextBBName << '|';
    return;
  }
}

void ReplayTrace::formatDeps(DynamicInstruction* DynamicInst,
                             std::ofstream& Out) {
  // if (DynamicInst->getStaticInstruction() != nullptr) {
  //   DEBUG(llvm::errs()
  //               << "Format dependence for "
  //               << DynamicInst->getStaticInstruction()->getName() << '\n');
  // }
  {
    auto DepIter = this->Trace->RegDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->RegDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->MemDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->MemDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->CtrDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->CtrDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
}

void ReplayTrace::formatOpCode(DynamicInstruction* DynamicInst,
                               std::ofstream& Out) {
  llvm::Instruction* StaticInstruction = DynamicInst->getStaticInstruction();

  // DEBUG(llvm::errs() << "Format " << DynamicInst->getOpName() << '\n');
  if (StaticInstruction == nullptr) {
    // Self defined instruction.
    Out << DynamicInst->getOpName();
    return;
  }
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