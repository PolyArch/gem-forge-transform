#include "stream/MemStream.h"

void MemStream::searchAddressComputeInstructions(
    std::function<bool(const llvm::PHINode *)> IsInductionVar) {

  std::list<llvm::Instruction *> Queue;

  llvm::Value *AddrValue = nullptr;
  if (llvm::isa<llvm::LoadInst>(this->Inst)) {
    AddrValue = this->Inst->getOperand(0);
  } else {
    AddrValue = this->Inst->getOperand(1);
  }

  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(AddrValue)) {
    Queue.emplace_back(AddrInst);
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    if (this->AddrInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      continue;
    }
    if (!this->Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    if (auto BaseLoad = llvm::dyn_cast<llvm::LoadInst>(CurrentInst)) {
      // This is also a base load.
      this->BaseLoads.insert(BaseLoad);
      // Do not go further for load.
      continue;
    }

    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(CurrentInst)) {
      if (IsInductionVar(PHINode)) {
        this->BaseInductionVars.insert(PHINode);
        // Do not go further for induction variables.
        continue;
      }
    }

    this->AddrInsts.insert(CurrentInst);
    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        // This is an instruction within the same context.
        Queue.emplace_back(OperandInst);
      }
    }
  }
}

void MemStream::formatAdditionalInfoText(std::ostream &OStream) const {
  this->AddrDG.format(OStream);
}

void MemStream::generateComputeFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  this->AddrDG.generateComputeFunction(this->AddressFunctionName, Module);
}