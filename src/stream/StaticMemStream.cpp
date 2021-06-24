#include "StaticMemStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "StaticMemStream"

StaticMemStream::StaticMemStream(const llvm::Instruction *_Inst,
                                 const llvm::Loop *_ConfigureLoop,
                                 const llvm::Loop *_InnerMostLoop,
                                 llvm::ScalarEvolution *_SE,
                                 const llvm::PostDominatorTree *_PDT,
                                 llvm::DataLayout *_DataLayout)
    : StaticStream(TypeT::MEM, _Inst, _ConfigureLoop, _InnerMostLoop, _SE, _PDT,
                   _DataLayout) {
  // We initialize the AddrDG, assuming all PHI nodes in loop head are IV
  // streams.
  auto IsInductionVar = [this](const llvm::PHINode *PHINode) -> bool {
    auto BB = PHINode->getParent();
    for (auto Loop : this->ConfigureLoop->getLoopsInPreorder()) {
      if (Loop->getHeader() == BB) {
        return true;
      }
    }
    return false;
  };
  this->AddrDG = std::make_unique<StreamDataGraph>(
      this->ConfigureLoop, Utils::getMemAddrValue(this->Inst), IsInductionVar);

  if (llvm::isa<llvm::StoreInst>(this->Inst)) {
    // StoreStream always has no core user.
    this->StaticStreamInfo.set_no_core_user(true);
  }
}

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
   * Hack: At this point, we know an AtomicStream has ValueDG or not.
   * If not, we mark NoCoreUser.
   * TODO: Find a better place to do this.
   */

  if (llvm::isa<llvm::AtomicCmpXchgInst>(this->Inst) ||
      llvm::isa<llvm::AtomicRMWInst>(this->Inst)) {
    if (!this->ValueDG) {
      this->StaticStreamInfo.set_no_core_user(true);
    }
  }

  /**
   * So far only look at inner most loop.
   * Ignore streams in remainder/epilogue loop.
   */
  if (LoopUtils::isLoopRemainderOrEpilogue(this->InnerMostLoop)) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::IN_LOOP_REMAINDER_OR_EPILOGUE);
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Loop Remainder/Epilogue "
                            << this->formatName() << '\n');
    return;
  }

  if (!this->checkBaseStreamInnerMostLoopContainsMine()) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::
            BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE);
    LLVM_DEBUG(llvm::dbgs()
               << "[NotCandidate]: Base Stream InnerMostLoop Not Contain Mine "
               << this->formatName() << '\n');
    return;
  }

  /**
   * Start to check constraints:
   * 1. No PHIMetaNode.
   */
  if (!this->PHIMetaNodes.empty()) {
    this->IsCandidate = false;
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: No PhiMetaNode "
                            << this->formatName() << '\n');
    return;
  }
  /**
   * At most one StepRootStream.
   */
  if (this->BaseStepRootStreams.size() > 1) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::MULTI_STEP_ROOT);
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Multiple Step Root "
                            << this->formatName() << '\n');
    return;
  }
  /**
   * ! Disable constant stream so far.
   */
  if (this->BaseStepRootStreams.size() == 0) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STEP_ROOT);
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: No Step Root "
                            << this->formatName() << '\n');
    return;
  }
  /**
   * For store stream, ignore update streams.
   */
  if (llvm::isa<llvm::StoreInst>(this->Inst)) {
    if (!StreamPassEnableStore) {
      this->IsCandidate = false;
      LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Store Disabled "
                              << this->formatName() << '\n');
      return;
    }
    if (this->UpdateStream) {
      // We keep the StoreStream if it has ValueDG, and we do not upgreade load
      // to update.
      if (!this->ValueDG || StreamPassUpgradeLoadToUpdate) {
        this->StaticStreamInfo.set_not_stream_reason(
            ::LLVM::TDG::StaticStreamInfo::IS_UPDATE_STORE);
        this->IsCandidate = false;
        LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Update Disabled "
                                << this->formatName() << '\n');
        return;
      }
    }
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
      LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Not SCEVable LoadBase "
                              << this->formatName() << '\n');
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &IndVarBaseStream : this->IndVarBaseStreams) {
    auto Inst = const_cast<llvm::Instruction *>(IndVarBaseStream->Inst);
    if (!this->SE->isSCEVable(Inst->getType())) {
      this->IsCandidate = false;
      LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Not SCEVable IVBase "
                              << this->formatName() << '\n');
      return;
    }
    InputSCEVs.insert(this->SE->getSCEV(Inst));
  }
  for (auto &LoopInvariantInput : this->LoopInvariantInputs) {
    if (!this->SE->isSCEVable(LoopInvariantInput->getType())) {
      this->IsCandidate = false;
      LLVM_DEBUG(llvm::dbgs()
                 << "[NotCandidate]: Not SCEVable LoopInvariantInput "
                 << this->formatName() << '\n');
      return;
    }
    InputSCEVs.insert(
        this->SE->getSCEV(const_cast<llvm::Value *>(LoopInvariantInput)));
  }
  if (!this->validateSCEVAsStreamDG(SCEV, InputSCEVs)) {
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Invalid SCEVStreamDG "
                            << this->formatName() << " SCEV: ";
               SCEV->print(llvm::dbgs()); llvm::dbgs() << '\n');

    this->IsCandidate = false;
    return;
  }

  if (this->isDirectMemStream()) {
    // For direct MemStream we want to enforce AddRecSCEV.
    if (!llvm::isa<llvm::SCEVAddRecExpr>(SCEV)) {
      LLVM_DEBUG(llvm::dbgs()
                     << "[NotCandidate]: DirectMemStream Requires AddRecSCEV "
                     << this->formatName() << " SCEV: ";
                 SCEV->print(llvm::dbgs()); llvm::dbgs() << '\n');
      this->IsCandidate = false;
      return;
    }
  }

  this->IsCandidate = true;
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
      continue;
    } else if (auto CastSCEV = llvm::dyn_cast<llvm::SCEVCastExpr>(SCEV)) {
      SCEV = CastSCEV->getOperand();
      continue;
    } else if (auto UDivSCEV = llvm::dyn_cast<llvm::SCEVUDivExpr>(SCEV)) {
      // We allow the divisor to be a power of 2.
      auto Divisor = UDivSCEV->getRHS();
      if (auto ConstantDivisor = llvm::dyn_cast<llvm::SCEVConstant>(Divisor)) {
        if (ConstantDivisor->getAPInt().isPowerOf2()) {
          SCEV = UDivSCEV->getLHS();
          continue;
        }
      }
    }
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: Invalid SCEV in StreamDG "
                            << this->formatName() << " SCEV: ";
               SCEV->print(llvm::dbgs()); llvm::dbgs() << '\n');
    return false;
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
    LLVM_DEBUG(llvm::dbgs()
               << "[UnQualify]: NoStaticMap " << this->formatName() << '\n');
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
    auto StepRootStreamStpPattern =
        StepRootStream->StaticStreamInfo.stp_pattern();
    LLVM_DEBUG(llvm::dbgs()
               << "Computed step pattern " << StepRootStreamStpPattern
               << " for " << this->formatName() << '\n');
    return StepRootStreamStpPattern;
  }
}

StaticMemStream::InputValueList
StaticMemStream::getAddrFuncInputValues() const {
  assert(this->isChosen() && "Only consider chosen stream's input values.");
  return this->getExecFuncInputValues(*this->AddrDG);
}

void StaticMemStream::constructChosenGraph() {
  StaticStream::constructChosenGraph();
  // We have some sanity checks here.
  if (this->ChosenBaseStepRootStreams.size() != 1) {
    llvm::errs() << "Invalid " << this->ChosenBaseStepRootStreams.size()
                 << " chosen StepRoot, " << this->BaseStepRootStreams.size()
                 << " StepRoot for " << this->formatName() << ":\n";
    for (auto StepRootS : this->ChosenBaseStepRootStreams) {
      llvm::errs() << "  " << StepRootS->formatName() << '\n';
    }
    assert(false);
  }
  for (auto StepRootS : this->ChosenBaseStepRootStreams) {
    if (StepRootS->ConfigureLoop != this->ConfigureLoop) {
      llvm::errs() << "StepRootStream is not configured at the same loop: "
                   << this->formatName() << " root " << StepRootS->formatName()
                   << '\n';
      assert(false);
    }
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

bool StaticMemStream::isDirectMemStream() const {
  if (!this->LoadBaseStreams.empty()) {
    // This is IndirectStream.
    return false;
  }
  if (this->IndVarBaseStreams.size() == 1) {
    // Check for PointerChase or PrevLoad.
    auto IVBaseS = *this->IndVarBaseStreams.begin();
    if (!IVBaseS->BackMemBaseStreams.empty()) {
      return false;
    }
  }
  return true;
}