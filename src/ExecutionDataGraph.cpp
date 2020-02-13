#include "ExecutionDataGraph.h"

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
  Function->setCallingConv(llvm::CallingConv::C);

  // Create the first and only basic block.
  auto &Context = Module->getContext();
  auto BB = llvm::BasicBlock::Create(Context, "entry", Function);
  llvm::IRBuilder<> Builder(BB);

  // Create the map from the original value to the new value.
  ValueMapT ValueMap;
  {
    auto ArgIter = Function->arg_begin();
    auto ArgEnd = Function->arg_end();
    for (const auto &Input : this->Inputs) {
      assert(ArgIter != ArgEnd &&
             "Mismatch between address daragraph input and function argument.");
      ValueMap.emplace(Input, ArgIter);
      ++ArgIter;
    }
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

  // Create the return inst.
  {
    auto ValueIter = ValueMap.find(this->ResultValue);
    assert(ValueIter != ValueMap.end() &&
           "Failed to find the translated result value.");
    Builder.CreateRet(ValueIter->second);
  }

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