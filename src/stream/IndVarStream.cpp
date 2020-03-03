#include "stream/IndVarStream.h"

#include <list>

#define DEBUG_TYPE "IndVarStream"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

IndVarStream::IndVarStream(const std::string &_Folder,
                           const std::string &_RelativeFolder,
                           const StaticStream *_SStream,
                           llvm::DataLayout *DataLayout)
    : Stream(_Folder, _RelativeFolder, _SStream, DataLayout),
      PHIInst(llvm::cast<llvm::PHINode>(_SStream->Inst)) {
  this->searchComputeInsts(this->PHIInst, this->getLoop());
  this->StepInsts =
      IndVarStream::searchStepInsts(this->PHIInst, this->getInnerMostLoop());
  this->IsCandidateStatic = this->isCandidateStatic();
}

bool IndVarStream::isCandidateStatic() const {
  if ((!this->PHIInst->getType()->isIntegerTy()) &&
      (!this->PHIInst->getType()->isPointerTy())) {
    return false;
  }
  for (const auto &ComputeInst : this->ComputeInsts) {
    switch (ComputeInst->getOpcode()) {
    case llvm::Instruction::Call:
    case llvm::Instruction::Invoke: {
      return false;
    }
    }
  }
  // Do not enable IVMemStream for now.
  bool EnableIVMemStream = true;
  if (EnableIVMemStream) {
    size_t BaseLoadInTheInnerMostLoop = 0;
    for (const auto &BaseLoad : this->BaseLoadInsts) {
      if (this->getInnerMostLoop()->contains(BaseLoad)) {
        BaseLoadInTheInnerMostLoop++;
      }
    }
    if (BaseLoadInTheInnerMostLoop > 1) {
      return false;
    }

    // For iv stream with base loads, we only allow it to be configured at inner
    // most loop level.
    if (BaseLoadInTheInnerMostLoop > 0 &&
        this->getLoop() != this->getInnerMostLoop()) {
      return false;
    }

  } else {
    if (!this->BaseLoadInsts.empty()) {
      return false;
    }
  }
  // Check whitelist.
  {
    auto StreamWhitelist = StreamUtils::getStreamWhitelist();
    if (StreamWhitelist.isInitialized()) {
      if (!StreamWhitelist.contains(this->SStream->Inst)) {
        return false;
      }
    }
  }

  // ! liblinear.
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "linear.c::844(solve_l2r_l1l2_svc)::bb218") {
    if (this->SStream->InnerMostLoop != this->SStream->ConfigureLoop) {
      return false;
    }
  }
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "linear.c::844(solve_l2r_l1l2_svc)::bb295") {
    if (this->SStream->InnerMostLoop != this->SStream->ConfigureLoop) {
      return false;
    }
  }
  return true;
}

void IndVarStream::searchComputeInsts(const llvm::PHINode *PHINode,
                                      const llvm::Loop *Loop) {
  std::list<llvm::Instruction *> Queue;

  LLVM_DEBUG(llvm::errs() << "Search compute instructions for "
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
      LLVM_DEBUG(llvm::errs()
                 << "Enqueue inst " << LoopUtils::formatLLVMInst(IncomingInst)
                 << '\n');
    }
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    LLVM_DEBUG(llvm::errs() << "Processing "
                            << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    if (this->ComputeInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      LLVM_DEBUG(llvm::errs() << "Already processed\n");
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      LLVM_DEBUG(llvm::errs() << "Not in loop\n");
      continue;
    }

    LLVM_DEBUG(llvm::errs() << "Found compute inst "
                            << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    this->ComputeInsts.insert(CurrentInst);

    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(CurrentInst)) {
      // We found a base load. Do not go further.
      this->BaseLoadInsts.insert(LoadInst);
      continue;
    }

    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        Queue.emplace_back(OperandInst);
        LLVM_DEBUG(llvm::errs()
                   << "Enqueue inst " << LoopUtils::formatLLVMInst(OperandInst)
                   << '\n');
      }
    }
  }
}

std::unordered_set<const llvm::Instruction *>
IndVarStream::searchStepInsts(const llvm::PHINode *PHINode,
                              const llvm::Loop *Loop) {
  std::list<llvm::Instruction *> Queue;

  LLVM_DEBUG(llvm::errs() << "Search step instructions for "
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
      LLVM_DEBUG(llvm::errs()
                 << "Enqueue inst " << LoopUtils::formatLLVMInst(IncomingInst)
                 << '\n');
    }
  }

  std::unordered_set<const llvm::Instruction *> StepInsts;
  std::unordered_set<const llvm::Instruction *> VisitedInsts;

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    LLVM_DEBUG(llvm::errs() << "Processing "
                            << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    if (VisitedInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      LLVM_DEBUG(llvm::errs() << "Already processed\n");
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      LLVM_DEBUG(llvm::errs() << "Not in loop\n");
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      LLVM_DEBUG(llvm::errs() << "Is call or invoke\n");
      continue;
    }

    VisitedInsts.insert(CurrentInst);

    if (Stream::isStepInst(CurrentInst)) {
      // Find a step instruction, do not go further.
      LLVM_DEBUG(llvm::errs()
                 << "Found step inst " << LoopUtils::formatLLVMInst(CurrentInst)
                 << '\n');
      StepInsts.insert(CurrentInst);
    } else if (llvm::isa<llvm::LoadInst>(CurrentInst)) {
      // Base load instruction is also considered as StepInst.
      StepInsts.insert(CurrentInst);
    } else {
      // BFS on the operands of non-step instructions.
      for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto OperandValue = CurrentInst->getOperand(OperandIdx);
        if (auto OperandInst =
                llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
          Queue.emplace_back(OperandInst);
          LLVM_DEBUG(llvm::errs()
                     << "Enqueue inst "
                     << LoopUtils::formatLLVMInst(OperandInst) << '\n');
        }
      }
    }
  }
  return StepInsts;
}

void IndVarStream::buildBasicDependenceGraph(GetStreamFuncT GetStream) {
  // Check the back edge.
  for (const auto &BaseLoad : this->BaseLoadInsts) {
    auto BaseMStream = GetStream(BaseLoad, this->getLoop());
    assert(BaseMStream != nullptr && "Failed to get back-edge MStream.");
    this->addBackEdgeBaseStream(BaseMStream);
  }
}

bool IndVarStream::isCandidate() const { return this->IsCandidateStatic; }

bool IndVarStream::isQualifySeed() const {
  if (!this->isCandidate()) {
    // I am not even a static candidate.
    return false;
  }
  // Check the dynamic behavior.
  if (!this->Pattern.computed()) {
    return false;
  }

  auto AddrPattern = this->Pattern.getPattern().ValPattern;
  if (AddrPattern > StreamPattern::ValuePattern::UNKNOWN &&
      AddrPattern <= StreamPattern::ValuePattern::QUARDRIC) {
    // This is an affine induction variable.
    return true;
  }

  /**
   * If I have back edges, consider myselfy as a candidate.
   */
  if (!this->BackMemBaseStreams.empty()) {
    return true;
  }
  return false;
}

Stream::InputValueList IndVarStream::getReduceFuncInputValues() const {
  assert(this->isChosen() && "Only consider chosen stream's input values.");
  assert(this->SStream->ReduceDG && "No ReduceDG.");
  return this->getExecFuncInputValues(*this->SStream->ReduceDG);
}

void IndVarStream::fillProtobufAddrFuncInfo(
    ::llvm::DataLayout *DataLayout,
    ::LLVM::TDG::ExecFuncInfo *AddrFuncInfo) const {

  if (!this->isChosen()) {
    return;
  }

  if (!this->SStream->ReduceDG) {
    return;
  }

  this->fillProtobufExecFuncInfo(DataLayout, AddrFuncInfo,
                                 this->SStream->FuncNameBase + "_reduce",
                                 *this->SStream->ReduceDG);
}