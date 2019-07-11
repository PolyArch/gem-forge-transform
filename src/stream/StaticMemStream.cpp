#include "StaticMemStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"

void StaticMemStream::initializeMetaGraphConstruction(
    std::list<DFSNode> &DFSStack,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  auto AddrValue =
      const_cast<llvm::Value *>(Utils::getMemAddrValue(this->Inst));
  const llvm::SCEV *SCEV = nullptr;
  if (this->SE->isSCEVable(AddrValue->getType())) {
    SCEV = this->SE->getSCEV(AddrValue);
  }
  this->ComputeMetaNodes.emplace_back(AddrValue, SCEV);
  ConstructedComputeMetaNodeMap.emplace(AddrValue,
                                        &this->ComputeMetaNodes.back());
  // Push to the stack.
  DFSStack.emplace_back(&this->ComputeMetaNodes.back(), AddrValue);
}

void StaticMemStream::analyzeIsCandidate() {
  /**
   * So far only look at inner most loop.
   */

  if (!this->checkBaseStreamInnerMostLoopContainsMine()) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::
            BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE);
    return;
  }

  /**
   * Start to check constraints:
   * 1. No PHIMetaNode.
   */
  if (!this->PHIMetaNodes.empty()) {
    this->IsCandidate = false;
    return;
  }
  /**
   * At most one StepRootStream.
   */
  if (this->BaseStepRootStreams.size() > 1) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::MULTI_STEP_ROOT);
    return;
  }
  assert(this->ComputeMetaNodes.size() == 1 &&
         "StaticMemStream should always have one ComputeMetaNode.");
  const auto &ComputeMNode = this->ComputeMetaNodes.front();
  auto SCEV = ComputeMNode.SCEV;
  assert(SCEV != nullptr && "StaticMemStream should always has SCEV.");
  /**
   * Validate the AddressComputeSCEV with InputSCEVs.
   */
  std::unordered_set<const llvm::SCEV *> InputSCEVs;
  for (auto &LoadBaseStream : this->LoadBaseStreams) {
    auto Inst = const_cast<llvm::Instruction *>(LoadBaseStream->Inst);
    if (!this->SE->isSCEVable(Inst->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &IndVarBaseStream : this->IndVarBaseStreams) {
    auto Inst = const_cast<llvm::Instruction *>(IndVarBaseStream->Inst);
    if (!this->SE->isSCEVable(Inst->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &LoopInvariantInput : this->LoopInvariantInputs) {
    if (!this->SE->isSCEVable(LoopInvariantInput->getType())) {
      this->IsCandidate = false;
      return;
    }
    InputSCEVs.insert(
        this->SE->getSCEV(const_cast<llvm::Value *>(LoopInvariantInput)));
  }
  if (this->validateSCEVAsStreamDG(SCEV, InputSCEVs)) {
    this->IsCandidate = true;
    return;
  }

  this->IsCandidate = false;
  return;
}

bool StaticMemStream::validateSCEVAsStreamDG(
    const llvm::SCEV *SCEV,
    const std::unordered_set<const llvm::SCEV *> &InputSCEVs) {
  while (true) {
    if (InputSCEVs.count(SCEV) != 0) {
      break;
    } else if (llvm::isa<llvm::SCEVAddRecExpr>(SCEV)) {
      break;
    } else if (auto ComSCEV = llvm::dyn_cast<llvm::SCEVCommutativeExpr>(SCEV)) {
      const llvm::SCEV *LoopVariantSCEV = nullptr;
      for (size_t OperandIdx = 0, NumOperands = ComSCEV->getNumOperands();
           OperandIdx < NumOperands; ++OperandIdx) {
        auto OpSCEV = ComSCEV->getOperand(OperandIdx);
        if (this->SE->isLoopInvariant(OpSCEV, this->ConfigureLoop)) {
          // Loop invariant.
          continue;
        }
        if (InputSCEVs.count(OpSCEV)) {
          // Input SCEV.
          continue;
        }
        if (llvm::isa<llvm::SCEVAddRecExpr>(OpSCEV)) {
          // AddRec SCEV.
          continue;
        }
        if (LoopVariantSCEV == nullptr) {
          LoopVariantSCEV = OpSCEV;
        } else {
          // More than one LoopVariant/Non-Input SCEV operand.
          return false;
        }
      }
      if (LoopVariantSCEV == nullptr) {
        // We are done.
        break;
      }
      SCEV = LoopVariantSCEV;
    } else if (auto CastSCEV = llvm::dyn_cast<llvm::SCEVCastExpr>(SCEV)) {
      SCEV = CastSCEV->getOperand();
    } else {
      return false;
    }
  }
  return true;
}

bool StaticMemStream::checkIsQualifiedWithoutBackEdgeDep() const {
  if (!this->isCandidate()) {
    return false;
  }
  // Check all the base streams are qualified.
  for (auto &BaseStream : this->BaseStreams) {
    if (!BaseStream->isQualified()) {
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::BASE_STREAM_NOT_QUALIFIED);
      return false;
    }
  }
  // All the base streams are qualified, and thus know their StepPattern. We can
  // check the constraints on static mapping.
  if (!this->checkStaticMapFromBaseStreamInParentLoop()) {
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STATIC_MAPPING);
    return false;
  }
  return true;
}

bool StaticMemStream::checkIsQualifiedWithBackEdgeDep() const {
  // MemStream should have no back edge dependence.
  assert(this->BackMemBaseStreams.empty() &&
         "Memory stream should not have back edge base stream.");
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

LLVM::TDG::StreamStepPattern StaticMemStream::computeStepPattern() const {
  // Copy the step pattern from the step root.
  if (this->BaseStepRootStreams.empty()) {
    return LLVM::TDG::StreamStepPattern::NEVER;
  } else {
    assert(this->BaseStepRootStreams.size() == 1 &&
           "More than one step root stream to finalize pattern.");
    auto StepRootStream = *(this->BaseStepRootStreams.begin());
    return StepRootStream->StaticStreamInfo.stp_pattern();
  }
}

void StaticMemStream::finalizePattern() {
  this->StaticStreamInfo.set_stp_pattern(this->computeStepPattern());
  // Compute the value pattern.
  this->StaticStreamInfo.set_val_pattern(
      LLVM::TDG::StreamValuePattern::CONSTANT);
  for (auto &BaseStream : this->BaseStreams) {
    if (BaseStream->Type == MEM) {
      // This makes me indirect stream.
      this->StaticStreamInfo.set_val_pattern(
          LLVM::TDG::StreamValuePattern::INDIRECT);
      break;
    } else {
      // This is likely our step root, copy its value pattern.
      this->StaticStreamInfo.set_val_pattern(
          BaseStream->StaticStreamInfo.val_pattern());
    }
  }
  // Compute the possible loop path.
  this->StaticStreamInfo.set_loop_possible_path(
      LoopUtils::countPossiblePath(this->InnerMostLoop));
  this->StaticStreamInfo.set_config_loop_possible_path(
      LoopUtils::countPossiblePath(this->ConfigureLoop));
}