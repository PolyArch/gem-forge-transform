#include "stream/ae/AddressDataGraph.h"

#include "LoopUtils.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include <list>
#include <vector>

AddressDataGraph::AddressDataGraph(
    const llvm::Loop *_Loop, const llvm::Value *_AddrValue,
    std::function<bool(const llvm::PHINode *)> IsInductionVar)
    : Loop(_Loop), AddrValue(_AddrValue), HasCircle(false) {
  this->constructDataGraph(IsInductionVar);
  this->HasCircle = this->detectCircle();
}

void AddressDataGraph::constructDataGraph(
    std::function<bool(const llvm::PHINode *)> IsInductionVar) {
  std::list<const llvm::Value *> Queue;
  Queue.emplace_back(this->AddrValue);

  std::unordered_set<const llvm::Value *> UnsortedInputs;

  while (!Queue.empty()) {
    auto Value = Queue.front();
    Queue.pop_front();
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Value);
    if (Inst == nullptr) {
      // This is not an instruction, should be an input value unless it's an
      // constant data.
      if (auto ConstantData = llvm::dyn_cast<llvm::ConstantData>(Value)) {
        this->ConstantDatas.insert(ConstantData);
        continue;
      }
      UnsortedInputs.insert(Value);
      continue;
    }
    if (!this->Loop->contains(Inst)) {
      // This is an instruction falling out of the loop, should also be an input
      // value.
      UnsortedInputs.insert(Inst);
      continue;
    }

    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
      if (IsInductionVar(PHINode)) {
        // This is an induction variable stream, should also be an input value.
        UnsortedInputs.insert(PHINode);
        this->BaseIVs.insert(PHINode);
        continue;
      }
    }

    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
      // This is a load stream, should also be an input value.
      UnsortedInputs.insert(LoadInst);
      this->BaseLoads.insert(LoadInst);
      continue;
    }

    // This is an compute instruction.
    bool Inserted = this->ComputeInsts.insert(Inst).second;
    if (Inserted) {
      // The instruction is inserted, which means that it is an new instruction.
      for (unsigned OperandIdx = 0, NumOperand = Inst->getNumOperands();
           OperandIdx != NumOperand; ++OperandIdx) {
        Queue.emplace_back(Inst->getOperand(OperandIdx));
      }
    }
  }

  /**
   * Generate the sorted inputs list.
   */
  this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                      UnsortedInputs.end());
}

void AddressDataGraph::format(std::ostream &OStream) const {
  OStream << "AddressDataGraph of Loop " << LoopUtils::getLoopId(this->Loop)
          << " Value " << LoopUtils::formatLLVMValue(this->AddrValue) << '\n';
  OStream << "-------------- Input Values ---------------\n";
  for (const auto &Input : this->Inputs) {
    OStream << LoopUtils::formatLLVMValue(Input) << '\n';
  }
  OStream << "-------------- Compute Insts --------------\n";
  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->AddrValue)) {
    if (this->ComputeInsts.count(AddrInst) == 0) {
      // This address instruction is somehow loop invariant.
      assert(this->ComputeInsts.empty() &&
             "If the address instruction is not in the loop, there should be "
             "no compute instructions.");
    } else {
      std::unordered_set<const llvm::Instruction *> FormattedInsts;
      auto FormatTask =
          [&OStream, &FormattedInsts](const llvm::Instruction *Inst) -> void {
        if (FormattedInsts.count(Inst) == 0) {
          OStream << LoopUtils::formatLLVMInst(Inst);
          for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            OStream << ' '
                    << LoopUtils::formatLLVMValue(Inst->getOperand(OperandIdx));
          }
          OStream << '\n';
          FormattedInsts.insert(Inst);
        }
      };
      if (this->HasCircle) {
        OStream << "!!! Circle !!!\n";
        for (const auto &ComputeInst : this->ComputeInsts) {
          FormatTask(ComputeInst);
        }
      } else {
        this->dfsOnComputeInsts(AddrInst, FormatTask);
      }
    }
  } else {
    assert(this->ComputeInsts.empty() &&
           "If there is no address instruction, there should be no compute "
           "instructions.");
  }
  OStream << "-------------- AddressDatagraph End -------\n";
}

bool AddressDataGraph::detectCircle() const {
  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->AddrValue)) {
    if (!this->ComputeInsts.empty()) {
      std::list<std::pair<const llvm::Instruction *, int>> Stack;
      Stack.emplace_back(AddrInst, 0);
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

void AddressDataGraph::dfsOnComputeInsts(const llvm::Instruction *Inst,
                                         DFSTaskT Task) const {
  assert(
      !this->HasCircle &&
      "Try to do DFS when there is circle in the address compute datagraph.");
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

llvm::Function *AddressDataGraph::generateComputeFunction(
    const std::string &FuncName, std::unique_ptr<llvm::Module> &Module) const {
  assert(!this->HasCircle &&
         "Can not create address function for cyclic address datagaph.");
  {
    auto TempFunc = Module->getFunction(FuncName);
    assert(TempFunc == nullptr && "Function is already inside the module.");
  }

  auto FuncType = this->createFunctionType(Module.get());
  auto Function = llvm::Function::Create(
      FuncType, llvm::GlobalValue::LinkageTypes::ExternalLinkage, FuncName,
      Module.get());
  assert(Function != nullptr && "Failed to insert the function.");

  // Create the first and only basic block.
  auto &Context = Module->getContext();
  auto BB = llvm::BasicBlock::Create(Context, "entry", Function);
  llvm::IRBuilder<> Builder(BB);

  // Create the map from the original value to the new value.
  std::unordered_map<const llvm::Value *, llvm::Value *> ValueMap;
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
    auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->AddrValue);
    assert(AddrInst != nullptr &&
           "AddrValue is not an instruction when we have a datagraph.");
    this->dfsOnComputeInsts(AddrInst, Translate);
  }

  // Create the return inst.
  {
    auto ValueIter = ValueMap.find(this->AddrValue);
    assert(ValueIter != ValueMap.end() &&
           "Failed to find the translated address value.");
    Builder.CreateRet(ValueIter->second);
  }

  return Function;
}

llvm::FunctionType *
AddressDataGraph::createFunctionType(llvm::Module *Module) const {
  std::vector<llvm::Type *> Args;
  for (const auto &Input : this->Inputs) {
    Args.emplace_back(Input->getType());
  }
  auto ResultType = this->AddrValue->getType();
  return llvm::FunctionType::get(ResultType, Args, false);
}

void AddressDataGraph::translate(
    llvm::IRBuilder<> &Builder,
    std::unordered_map<const llvm::Value *, llvm::Value *> &ValueMap,
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