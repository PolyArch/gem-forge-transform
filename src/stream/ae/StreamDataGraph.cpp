#include "stream/ae/StreamDataGraph.h"

#include "LoopUtils.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include <list>
#include <vector>

StreamDataGraph::StreamDataGraph(
    const llvm::Loop *_Loop, const llvm::Value *_AddrValue,
    std::function<bool(const llvm::PHINode *)> IsInductionVar)
    : ExecutionDataGraph(_AddrValue), Loop(_Loop),
      HasPHINodeInComputeInsts(false), HasCallInstInComputeInsts(false) {
  this->constructDataGraph(IsInductionVar);
  this->HasCircle = this->detectCircle();

  for (const auto &ComputeInst : this->ComputeInsts) {
    if (llvm::isa<llvm::PHINode>(ComputeInst)) {
      this->HasPHINodeInComputeInsts = true;
    }
    if (Utils::isCallOrInvokeInst(ComputeInst)) {
      this->HasCallInstInComputeInsts = true;
    }
  }
}

void StreamDataGraph::constructDataGraph(
    std::function<bool(const llvm::PHINode *)> IsInductionVar) {
  std::list<const llvm::Value *> Queue;
  Queue.emplace_back(this->ResultValue);

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
   * Generate the sorted inputs list, (actually just get a fixed order).
   */
  this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                      UnsortedInputs.end());
}

void StreamDataGraph::format(std::ostream &OStream) const {
  OStream << "StreamDataGraph of Loop " << LoopUtils::getLoopId(this->Loop)
          << " Value " << Utils::formatLLVMValue(this->ResultValue) << '\n';
  OStream << "-------------- Input Values ---------------\n";
  for (const auto &Input : this->Inputs) {
    OStream << Utils::formatLLVMValue(Input) << '\n';
  }
  OStream << "-------------- Compute Insts --------------\n";
  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->ResultValue)) {
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
          OStream << Utils::formatLLVMInst(Inst);
          for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            OStream << ' '
                    << Utils::formatLLVMValue(Inst->getOperand(OperandIdx));
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
  OStream << "-------------- StreamDataGraph End -------\n";
}

/**
 * Two address datagraphs can be coalesce iff.
 * 1.
 */
bool StreamDataGraph::isAbleToCoalesceWith(const StreamDataGraph &Other) const {
  return false;
}
