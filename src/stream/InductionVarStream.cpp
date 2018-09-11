#include "stream/InductionVarStream.h"

#include <list>

bool InductionVarStream::isInductionVarStream(
    const llvm::PHINode *PHINode,
    const std::unordered_set<const llvm::Instruction *> &ComputeInsts) {
  if (!PHINode->getType()->isIntegerTy()) {
    return false;
  }
  for (const auto &ComputeInst : ComputeInsts) {
    switch (ComputeInst->getOpcode()) {
    case llvm::Instruction::Call:
    case llvm::Instruction::Invoke:
    case llvm::Instruction::Store:
    case llvm::Instruction::Load: {
      return false;
    }
    }
  }
  return true;
}

std::unordered_set<const llvm::Instruction *>
InductionVarStream::searchComputeInsts(const llvm::PHINode *PHINode,
                                       const llvm::Loop *Loop) {

  std::list<llvm::Instruction *> Queue;

  for (unsigned IncomingIdx = 0,
                NumIncomingValues = PHINode->getNumIncomingValues();
       IncomingIdx != NumIncomingValues; ++IncomingIdx) {
    auto BB = PHINode->getIncomingBlock(IncomingIdx);
    if (!Loop->contains(BB)) {
      continue;
    }
    auto IncomingValue = PHINode->getIncomingValue(IncomingIdx);
    if (auto IncomingInst = llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
      Queue.emplace_back(IncomingInst);
    }
  }

  std::unordered_set<const llvm::Instruction *> ComputeInsts;

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    if (ComputeInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    ComputeInsts.insert(CurrentInst);

    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        Queue.emplace_back(OperandInst);
      }
    }
  }

  return std::move(ComputeInsts);
}
