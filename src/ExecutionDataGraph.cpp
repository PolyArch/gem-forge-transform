#include "ExecutionDataGraph.h"

void ExecutionDataGraph::extendTailAtomicRMW(
    const llvm::AtomicRMWInst *AtomicRMW) {
  assert(!this->TailAtomicRMW && "Already has TailAtomicRMW.");
  // The atomic RMW should has my result as the second value.
  if (AtomicRMW->getOperand(1) != this->ResultValue) {
    llvm::errs() << "TailAtomicRMW is not using my result.\n";
    assert(false);
  }
  if (AtomicRMW->getType() != this->ResultValue->getType()) {
    llvm::errs() << "TailAtomicRMW is of different type.\n";
    assert(false);
  }
  this->TailAtomicRMW = AtomicRMW;
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

llvm::FunctionType *
ExecutionDataGraph::createFunctionType(llvm::Module *Module) const {
  std::vector<llvm::Type *> Args;
  for (const auto &Input : this->Inputs) {
    Args.emplace_back(Input->getType());
  }
  auto ResultType = this->ResultValue->getType();
  // Add final input of the TailAtomicRMW.
  if (this->TailAtomicRMW) {
    auto AtomicRMWInputPtrType = this->TailAtomicRMW->getOperand(0)->getType();
    assert(AtomicRMWInputPtrType->isPointerTy() && "Should be pointer.");
    Args.emplace_back(AtomicRMWInputPtrType->getPointerElementType());
    ResultType = this->TailAtomicRMW->getType();
  }
  return llvm::FunctionType::get(ResultType, Args, false);
}

llvm::Function *ExecutionDataGraph::generateComputeFunction(
    const std::string &FuncName, std::unique_ptr<llvm::Module> &Module) const {
  {
    auto TempFunc = Module->getFunction(FuncName);
    assert(TempFunc == nullptr && "Function is already inside the module.");
  }

  auto FuncType = this->createFunctionType(Module.get());
  auto Function = llvm::Function::Create(
      FuncType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, FuncName,
      Module.get());
  assert(Function != nullptr && "Failed to insert the function.");
  // Make sure we are using C calling convention.
  Function->setCallingConv(llvm::CallingConv::X86_64_SysV);
  // Copy the "target-features" attributes from the original function.
  if (auto ResultInst = llvm::dyn_cast<llvm::Instruction>(this->ResultValue)) {
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
    for (const auto &ConstantData : this->ConstantDatas) {
      // This is hacky...
      ValueMap.emplace(ConstantData,
                       const_cast<llvm::Value *>(
                           static_cast<const llvm::Value *>(ConstantData)));
    }
  }

  // Start to translate the datagraph into the function body.
  if (!this->ComputeInsts.empty()) {
    auto Translate = [this, &Builder,
                      &ValueMap](const llvm::Instruction *Inst) -> void {
      this->translate(Builder, ValueMap, Inst);
    };
    auto ResultInst = llvm::dyn_cast<llvm::Instruction>(this->ResultValue);
    assert(ResultInst != nullptr &&
           "ResultValue is not an instruction when we have a datagraph.");
    this->dfsOnComputeInsts(ResultInst, Translate);
  }

  auto FinalValueIter = ValueMap.find(this->ResultValue);
  assert(FinalValueIter != ValueMap.end() &&
         "Failed to find the translated result value.");
  llvm::Value *FinalValue = FinalValueIter->second;

  // Create the tail AtomicRMW.
  if (this->TailAtomicRMW) {
    assert(ArgIter != ArgEnd && "Missing AtomicInput.");
    llvm::Value *AtomicArg = ArgIter;
    switch (this->TailAtomicRMW->getOperation()) {
    default:
      llvm_unreachable("Unsupported TailAtomicRMW Operation.");
    case llvm::AtomicRMWInst::FAdd:
      FinalValue = Builder.CreateFAdd(AtomicArg, FinalValue, "atomicrmw");
      break;
    case llvm::AtomicRMWInst::Add:
      FinalValue = Builder.CreateAdd(AtomicArg, FinalValue, "atomicrmw");
      break;
    }
  }

  // Create the return inst.
  Builder.CreateRet(FinalValue);

  return Function;
}

void ExecutionDataGraph::translate(llvm::IRBuilder<> &Builder,
                                   ValueMapT &ValueMap,
                                   const llvm::Instruction *Inst) const {
  if (ValueMap.count(Inst) != 0) {
    // We have already translated this instruction.
    return;
  }
  auto NewInst = Inst->clone();
  // Fix the operand.
  for (unsigned OperandIdx = 0, NumOperands = NewInst->getNumOperands();
       OperandIdx != NumOperands; ++OperandIdx) {
    auto ValueIter = ValueMap.find(Inst->getOperand(OperandIdx));
    assert(ValueIter != ValueMap.end() && "Failed to find translated operand.");
    NewInst->setOperand(OperandIdx, ValueIter->second);
  }
  // Insert with the same name.
  Builder.Insert(NewInst, Inst->getName());
  // Insert into the value map.
  ValueMap.emplace(Inst, NewInst);
}

bool ExecutionDataGraph::detectCircle() const {
  if (auto ResultInst = llvm::dyn_cast<llvm::Instruction>(this->ResultValue)) {
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