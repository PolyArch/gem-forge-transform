#include "stream/InductionVarStream.h"

#include <list>

#define DEBUG_TYPE "InductionVarStream"

InductionVarStream::InductionVarStream(const std::string &_Folder,
                                       const llvm::PHINode *_PHIInst,
                                       const llvm::Loop *_Loop,
                                       const llvm::Loop *_InnerMostLoop,
                                       size_t _Level)
    : Stream(TypeT::IV, _Folder, _PHIInst, _Loop, _InnerMostLoop, _Level),
      PHIInst(_PHIInst) {
  this->ComputeInsts =
      InductionVarStream::searchComputeInsts(this->PHIInst, this->Loop);
  this->StepInsts =
      InductionVarStream::searchStepInsts(this->PHIInst, this->InnerMostLoop);
  this->IsCandidate = InductionVarStream::isInductionVarStream(
      this->PHIInst, this->ComputeInsts);
}

bool InductionVarStream::isInductionVarStream(
    const llvm::PHINode *PHINode,
    const std::unordered_set<const llvm::Instruction *> &ComputeInsts) {
  if ((!PHINode->getType()->isIntegerTy()) &&
      (!PHINode->getType()->isPointerTy())) {
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

  DEBUG(llvm::errs() << "Search compute instructions for "
                     << LoopUtils::formatLLVMInst(PHINode) << " at loop "
                     << LoopUtils::getLoopId(Loop) << '\n');

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
      DEBUG(llvm::errs() << "Enqueue inst "
                         << LoopUtils::formatLLVMInst(IncomingInst) << '\n');
    }
  }

  std::unordered_set<const llvm::Instruction *> ComputeInsts;

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    DEBUG(llvm::errs() << "Processing "
                       << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    if (ComputeInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      DEBUG(llvm::errs() << "Already processed\n");
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      DEBUG(llvm::errs() << "Not in loop\n");
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      DEBUG(llvm::errs() << "Is call or invoke\n");
      continue;
    }

    DEBUG(llvm::errs() << "Found compute inst "
                       << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    ComputeInsts.insert(CurrentInst);

    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        Queue.emplace_back(OperandInst);
        DEBUG(llvm::errs() << "Enqueue inst "
                           << LoopUtils::formatLLVMInst(OperandInst) << '\n');
      }
    }
  }
  return ComputeInsts;
}

std::unordered_set<const llvm::Instruction *>
InductionVarStream::searchStepInsts(const llvm::PHINode *PHINode,
                                    const llvm::Loop *Loop) {

  std::list<llvm::Instruction *> Queue;

  DEBUG(llvm::errs() << "Search step instructions for "
                     << LoopUtils::formatLLVMInst(PHINode) << " at loop "
                     << LoopUtils::getLoopId(Loop) << '\n');

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
      DEBUG(llvm::errs() << "Enqueue inst "
                         << LoopUtils::formatLLVMInst(IncomingInst) << '\n');
    }
  }

  std::unordered_set<const llvm::Instruction *> StepInsts;
  std::unordered_set<const llvm::Instruction *> VisitedInsts;

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    DEBUG(llvm::errs() << "Processing "
                       << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    if (VisitedInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      DEBUG(llvm::errs() << "Already processed\n");
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      DEBUG(llvm::errs() << "Not in loop\n");
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      DEBUG(llvm::errs() << "Is call or invoke\n");
      continue;
    }

    VisitedInsts.insert(CurrentInst);

    if (Stream::isStepInst(CurrentInst)) {
      // Find a step instruction, do not go further.
      DEBUG(llvm::errs() << "Found step inst "
                         << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
      StepInsts.insert(CurrentInst);
    } else {
      // BFS on the operands of non-step instructions.
      for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto OperandValue = CurrentInst->getOperand(OperandIdx);
        if (auto OperandInst =
                llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
          Queue.emplace_back(OperandInst);
          DEBUG(llvm::errs() << "Enqueue inst "
                             << LoopUtils::formatLLVMInst(OperandInst) << '\n');
        }
      }
    }
  }
  return StepInsts;
}

#undef DEBUG_TYPE