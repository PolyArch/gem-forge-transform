#include "ExecutionDataGraph.h"

#include "Utils.h"

#define DEBUG_TYPE "ExecutionDataGraph"

namespace {
// A utility function copied from upstream llvm.
bool getIntrinsicSignature(llvm::Function *F,
                           llvm::SmallVectorImpl<llvm::Type *> &ArgTys) {
  llvm::Intrinsic::ID ID = F->getIntrinsicID();
  if (!ID)
    return false;

  llvm::SmallVector<llvm::Intrinsic::IITDescriptor, 8> Table;
  llvm::Intrinsic::getIntrinsicInfoTableEntries(ID, Table);
  llvm::ArrayRef<llvm::Intrinsic::IITDescriptor> TableRef = Table;

  if (llvm::Intrinsic::matchIntrinsicSignature(F->getFunctionType(), TableRef,
                                               ArgTys) !=
      llvm::Intrinsic::MatchIntrinsicTypesResult::MatchIntrinsicTypes_Match) {
    return false;
  }
  if (llvm::Intrinsic::matchIntrinsicVarArg(F->getFunctionType()->isVarArg(),
                                            TableRef))
    return false;
  return true;
}
} // namespace

void ExecutionDataGraph::extendTailAtomicInst(
    const llvm::Instruction *AtomicInst,
    const std::vector<const llvm::Instruction *> &FusedLoadOps) {
  assert(!this->TailAtomicInst && "Already has TailAtomicInst.");
  // Sanity check for the atomic instruction.
  if (AtomicInst->getOpcode() == llvm::Instruction::AtomicRMW) {
    // AtomicRMW: should has my result as the second value.
    if (AtomicInst->getOperand(1) != this->getSingleResultValue()) {
      llvm::errs() << "TailAtomicRMW is not using my result.\n";
      assert(false);
    }
    if (AtomicInst->getType() != this->getSingleResultValue()->getType()) {
      llvm::errs() << "TailAtomicRMW is of different type.\n";
      assert(false);
    }
  } else if (AtomicInst->getOpcode() == llvm::Instruction::AtomicCmpXchg) {
    // AtomicCmpXchg: should has my result as the third value, and the compared
    //   value should be a constant.
    if (AtomicInst->getOperand(2) != this->getSingleResultValue()) {
      llvm::errs() << "TailAtomicCmpXchg is not using my result.\n";
      assert(false);
    }
    const auto ComparedValue = AtomicInst->getOperand(1);
    if (!llvm::isa<llvm::Constant>(ComparedValue)) {
      llvm::errs()
          << "The compared value of AtomicCmpXchg should be a constant.";
      assert(false);
    }
  } else {
    llvm_unreachable("Invalid tail atomic instruction.");
  }
  this->TailAtomicInst = AtomicInst;
  this->FusedLoadOps = FusedLoadOps;
}

void ExecutionDataGraph::dfsOnComputeInsts(const llvm::Instruction *Inst,
                                           DFSTaskT Task) const {
  assert(!this->HasCircle && "Doing DFS on circular ExecutionDataGraph.");
  assert(this->ComputeInsts.count(Inst) != 0 &&
         "This instruction is not in the compute instructions.");

  for (unsigned OperandIdx = 0, NumOperand = Inst->getNumOperands();
       OperandIdx != NumOperand; ++OperandIdx) {
    if (auto OperandInst =
            llvm::dyn_cast<llvm::Instruction>(Inst->getOperand(OperandIdx))) {
      if (this->ComputeInsts.count(OperandInst) != 0) {
        this->dfsOnComputeInsts(OperandInst, Task);
      }
    }
  }
  Task(Inst);
}

llvm::Type *ExecutionDataGraph::getReturnType(bool IsLoad) const {
  if (this->hasSingleResult()) {
    auto ResultType = this->getSingleResultValue()->getType();
    if (this->TailAtomicInst) {
      /**
       * For TailAtomicInst, the only exception is the load function for
       * CmpXchg.
       */
      if (this->TailAtomicInst->getOpcode() ==
          llvm::Instruction::AtomicCmpXchg) {
        if (IsLoad) {
          ResultType = this->TailAtomicInst->getType();
        }
      }
    }
    // However, if there is fused load ops, then it should be the final value.
    if (IsLoad && !this->FusedLoadOps.empty()) {
      ResultType = this->FusedLoadOps.back()->getType();
    }
    return ResultType;
  } else if (this->hasMultipleResult()) {
    // Simply return void.
    return llvm::Type::getVoidTy(this->ResultValues.back()->getContext());
  } else {
    assert(false && "No ResultValue.");
  }
}

llvm::FunctionType *ExecutionDataGraph::createFunctionType(llvm::Module *Module,
                                                           bool IsLoad) const {
  std::vector<llvm::Type *> Args;
  for (const auto &Input : this->Inputs) {
    Args.emplace_back(Input->getType());
  }
  if (this->TailAtomicInst) {
    // Add additional input of the TailAtomicInst.
    auto AtomicRMWInputPtrType = this->TailAtomicInst->getOperand(0)->getType();
    assert(AtomicRMWInputPtrType->isPointerTy() && "Should be pointer.");
    Args.emplace_back(AtomicRMWInputPtrType->getPointerElementType());
  }
  auto ResultType = this->getReturnType(IsLoad);
  return llvm::FunctionType::get(ResultType, Args, false);
}

llvm::Function *
ExecutionDataGraph::generateFunction(const std::string &FuncName,
                                     std::unique_ptr<llvm::Module> &Module,
                                     bool IsLoad) const {
  {
    auto TempFunc = Module->getFunction(FuncName);
    assert(TempFunc == nullptr && "Function is already inside the module.");
  }

  auto FuncType = this->createFunctionType(Module.get(), IsLoad);
  auto Function = llvm::Function::Create(
      FuncType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, FuncName,
      Module.get());
  assert(Function != nullptr && "Failed to insert the function.");
  // Make sure we are using C calling convention.
  Function->setCallingConv(llvm::CallingConv::X86_64_SysV);
  // Copy the "target-features" attributes from the original function.
  if (auto ResultInst =
          llvm::dyn_cast<llvm::Instruction>(this->ResultValues.back())) {
    auto OriginalFunction = ResultInst->getFunction();
    for (auto &AttrSet : OriginalFunction->getAttributes()) {
      if (AttrSet.hasAttribute("target-features")) {
        Function->addFnAttr(AttrSet.getAttribute("target-features"));
        break;
      }
    }
  }

  // Create the first and only basic block.
  auto &Context = Module->getContext();
  auto BB = llvm::BasicBlock::Create(Context, "entry", Function);
  llvm::IRBuilder<> Builder(BB);

  // Create the map from the original value to the new value.
  ValueMapT ValueMap;
  auto ArgIter = Function->arg_begin();
  auto ArgEnd = Function->arg_end();
  for (const auto &Input : this->Inputs) {
    assert(ArgIter != ArgEnd &&
           "Mismatch between address daragraph input and function argument.");
    ValueMap.emplace(Input, ArgIter);
    ++ArgIter;
  }
  {
    // Remember to translate the constant value to themselves.
    for (const auto &ConstantData : this->ConstantValues) {
      // This is hacky...
      ValueMap.emplace(ConstantData,
                       const_cast<llvm::Value *>(
                           static_cast<const llvm::Value *>(ConstantData)));
    }
  }

  // Start to translate the datagraph into the function body.
  if (!this->ComputeInsts.empty()) {
    auto Translate = [this, &Module, &Builder,
                      &ValueMap](const llvm::Instruction *Inst) -> void {
      this->translate(Module, Builder, ValueMap, Inst);
    };
    for (auto ResultValue : this->ResultValues) {
      auto ResultInst = llvm::dyn_cast<llvm::Instruction>(ResultValue);
      assert(ResultInst != nullptr &&
             "ResultValue is not an instruction when we have a datagraph.");
      this->dfsOnComputeInsts(ResultInst, Translate);
    }
  }

  if (this->hasMultipleResult()) {
    // Simply return a void.
    Builder.CreateRetVoid();
    return Function;
  }

  auto FinalValueIter = ValueMap.find(this->getSingleResultValue());
  assert(FinalValueIter != ValueMap.end() &&
         "Failed to find the translated result value.");
  llvm::Value *FinalValue = FinalValueIter->second;

  // Create the tail AtomicRMW.
  if (this->TailAtomicInst) {
    assert(ArgIter != ArgEnd && "Missing AtomicInput.");
    llvm::Value *AtomicArg = ArgIter;
    ++ArgIter;
    if (this->TailAtomicInst->getOpcode() == llvm::Instruction::AtomicRMW) {
      FinalValue =
          this->generateTailAtomicRMW(Builder, AtomicArg, FinalValue, IsLoad);
    } else {
      // The compared value is always a constant, for now, so we use it
      // directly.
      auto CmpValue = this->TailAtomicInst->getOperand(1);
      FinalValue = this->generateTailAtomicCmpXchg(Builder, AtomicArg, CmpValue,
                                                   FinalValue, IsLoad);
    }
    ValueMap.emplace(this->TailAtomicInst, FinalValue);
  }

  // Handle LoadFusedOps for load function.
  if (IsLoad) {
    for (auto Op : this->FusedLoadOps) {
      this->translate(Module, Builder, ValueMap, Op);
      FinalValue = ValueMap.at(Op);
    }
  }

  // Create the return inst.
  Builder.CreateRet(FinalValue);

  return Function;
}

void ExecutionDataGraph::translate(std::unique_ptr<llvm::Module> &Module,
                                   llvm::IRBuilder<> &Builder,
                                   ValueMapT &ValueMap,
                                   const llvm::Instruction *Inst) const {
  if (ValueMap.count(Inst) != 0) {
    // We have already translated this instruction.
    return;
  }
  auto NewInst = Inst->clone();
  // Fix the operand.
  LLVM_DEBUG({
    llvm::dbgs() << "Translating Inst";
    Inst->print(llvm::dbgs());
    llvm::dbgs() << '\n';
  });
  for (unsigned OperandIdx = 0, NumOperands = NewInst->getNumOperands();
       OperandIdx != NumOperands; ++OperandIdx) {
    auto Operand = Inst->getOperand(OperandIdx);
    // Directly use the constant data and intrinsic.
    if (llvm::isa<llvm::ConstantData>(Operand)) {
      LLVM_DEBUG({
        llvm::dbgs() << "Directly use ConstantData";
        Operand->print(llvm::dbgs());
        llvm::dbgs() << '\n';
      });
    }
    if (llvm::isa<llvm::ConstantVector>(Operand)) {
      LLVM_DEBUG({
        llvm::dbgs() << "Directly use ConstantVector";
        Operand->print(llvm::dbgs());
        llvm::dbgs() << '\n';
      });
    }
    if (llvm::isa<llvm::IntrinsicInst>(Inst) && OperandIdx + 1 == NumOperands) {
      LLVM_DEBUG({
        llvm::dbgs() << "Directly use Intrinsic";
        Operand->print(llvm::dbgs());
        llvm::dbgs() << '\n';
      });
      NewInst->setOperand(OperandIdx, Operand);
      continue;
    }
    auto ValueIter = ValueMap.find(Operand);
    if (ValueIter == ValueMap.end()) {
      llvm::errs() << "Failed to find translated operand #" << OperandIdx
                   << " ";
      Operand->print(llvm::errs());
      llvm::errs() << " of Inst ";
      Inst->print(llvm::errs());
      llvm::errs() << '\n';
      assert(false && "Failed to find translated operand.");
    }
    NewInst->setOperand(OperandIdx, ValueIter->second);
  }
  // Fix the callee.
  if (auto NewCallInst = llvm::dyn_cast<llvm::CallInst>(NewInst)) {
    assert(NewCallInst->getIntrinsicID() != llvm::Intrinsic::not_intrinsic &&
           "NonIntrinsicCall in ExecutionDataGraph.");
    LLVM_DEBUG({
      llvm::dbgs() << "Fix Intrinsic";
      NewInst->print(llvm::dbgs());
      llvm::dbgs() << '\n';
    });
    llvm::SmallVector<llvm::Type *, 4> OverloadedTypes;
    getIntrinsicSignature(NewCallInst->getCalledFunction(), OverloadedTypes);
    // Translate the IntrinsicCallee.
    auto NewCallee = llvm::Intrinsic::getDeclaration(
        Module.get(), NewCallInst->getIntrinsicID(), OverloadedTypes);
    NewCallInst->setCalledFunction(NewCallee);
  }
  // Insert with the same name.
  Builder.Insert(NewInst, Inst->getName());
  // Insert into the value map.
  ValueMap.emplace(Inst, NewInst);
}

bool ExecutionDataGraph::detectCircle() const {
  for (auto ResultValue : this->ResultValues) {
    auto ResultInst = llvm::dyn_cast<llvm::Instruction>(ResultValue);
    if (!ResultInst) {
      continue;
    }
    if (!this->ComputeInsts.empty()) {
      std::list<std::pair<const llvm::Instruction *, int>> Stack;
      Stack.emplace_back(ResultInst, 0);
      while (!Stack.empty()) {
        auto &Entry = Stack.back();
        auto &Inst = Entry.first;
        if (Entry.second == 0) {
          // First time.
          Entry.second = 1;
          for (unsigned OperandIdx = 0, NumOperand = Inst->getNumOperands();
               OperandIdx != NumOperand; ++OperandIdx) {
            auto OperandValue = Inst->getOperand(OperandIdx);
            if (auto OperandInst =
                    llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
              if (this->ComputeInsts.count(OperandInst) != 0) {
                // Check if this instruction is already in the stack.
                for (const auto &StackElement : Stack) {
                  if (StackElement.first == OperandInst &&
                      StackElement.second == 1) {
                    // There is circle.
                    return true;
                  }
                }
                Stack.emplace_back(OperandInst, 0);
              }
            }
          }
        } else {
          // Second time.
          Stack.pop_back();
        }
      }
    }
  }
  return false;
}

llvm::Value *ExecutionDataGraph::generateTailAtomicRMW(
    llvm::IRBuilder<> &Builder, llvm::Value *AtomicArg, llvm::Value *RhsArg,
    bool IsLoad) const {
  auto RMWInst = llvm::dyn_cast<llvm::AtomicRMWInst>(this->TailAtomicInst);
  assert(RMWInst && "Not an AtomicRMW instruction.");
  if (IsLoad) {
    // The load function will directly return the atomic value.
    return AtomicArg;
  }
  switch (RMWInst->getOperation()) {
  default:
    llvm::errs() << "Unsupported TailAtomicRMW Operation "
                 << Utils::formatLLVMInst(RMWInst) << '\n';
    llvm_unreachable("Unsupported TailAtomicRMW Operation.");
  case llvm::AtomicRMWInst::FAdd:
    return Builder.CreateFAdd(AtomicArg, RhsArg, "atomicrmw");
  case llvm::AtomicRMWInst::Add:
    return Builder.CreateAdd(AtomicArg, RhsArg, "atomicrmw");
    // LLVM has no instr/intrinsic for integer min/max...
  case llvm::AtomicRMWInst::UMin: {
    auto CmpValue = Builder.CreateICmpULT(AtomicArg, RhsArg, "umincmp");
    return Builder.CreateSelect(CmpValue, AtomicArg, RhsArg, "atomicrmw");
  }
  case llvm::AtomicRMWInst::Min: {
    auto CmpValue = Builder.CreateICmpSLT(AtomicArg, RhsArg, "mincmp");
    return Builder.CreateSelect(CmpValue, AtomicArg, RhsArg, "atomicrmw");
  }
  }
}

llvm::Value *ExecutionDataGraph::generateTailAtomicCmpXchg(
    llvm::IRBuilder<> &Builder, llvm::Value *AtomicArg, llvm::Value *CmpValue,
    llvm::Value *XchgValue, bool IsLoad) const {
  auto CmpXchgInst =
      llvm::dyn_cast<llvm::AtomicCmpXchgInst>(this->TailAtomicInst);
  assert(CmpXchgInst && "Not an AtomicCmpXchg instruction.");
  auto MatchValue = Builder.CreateICmpEQ(AtomicArg, CmpValue, "cmpeq");
  if (IsLoad) {
    // Create the loaded value.
    auto InsertValue =
        Builder.CreateInsertValue(llvm::UndefValue::get(CmpXchgInst->getType()),
                                  AtomicArg, {0}, "insert");
    return Builder.CreateInsertValue(InsertValue, MatchValue, {1}, "cmpxchg");
  } else {
    // Store version will just get the selected value.
    return Builder.CreateSelect(MatchValue, XchgValue, AtomicArg, "cmpxchg");
  }
}

::LLVM::TDG::ExecFuncInfo::ComputeOp ExecutionDataGraph::getComputeOp() const {
  auto Op = ::LLVM::TDG::ExecFuncInfo_ComputeOp_UNKNOWN;
  if (this->ComputeInsts.size() == 1) {
    auto ComputeInst = *this->ComputeInsts.begin();
    switch (ComputeInst->getOpcode()) {
    default:
      break;
    case llvm::Instruction::FAdd: {
      if (ComputeInst->getType()->isFloatTy()) {
        // So far we just support float addition.
        Op = ::LLVM::TDG::ExecFuncInfo_ComputeOp_FLOAT_ADD;
      }
      break;
    }
    case llvm::Instruction::Add: {
      if (ComputeInst->getType()->isIntegerTy()) {
        // So far we just support float addition.
        Op = ::LLVM::TDG::ExecFuncInfo_ComputeOp_INT_ADD;
      }
      break;
    }
    }
    return Op;
  }

  return Op;
}