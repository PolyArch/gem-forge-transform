#include "stream/StaticIndVarStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "StaticIndVarStream"

void StaticIndVarStream::constructMetaGraph(GetStreamFuncT GetStream) {
  StaticStream::constructMetaGraph(GetStream);

  // Initialize to RANDOM pattern.
  this->StaticStreamInfo.set_val_pattern(LLVM::TDG::StreamValuePattern::RANDOM);

  // After we construct MetaGraph, we can analyze ComputePath.
  for (const auto &ComputeMNode : this->ComputeMetaNodes) {
    if (ComputeMNode.PHIMetaNodes.size() > 1) {
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::MULTI_PHIMNODE_FOR_COMPUTEMNODE);
      return;
    }
  }

  this->AllComputePaths = this->constructComputePath();

  this->EmptyComputePathFound = false;
  LLVM_DEBUG(llvm::dbgs() << "[ComputePath] Analyze " << this->getStreamName()
                          << '\n');
  for (const auto &Path : AllComputePaths) {
    LLVM_DEBUG(Path.debug());
    if (Path.isEmpty()) {
      this->EmptyComputePathFound = true;
      continue;
    }
    if (this->NonEmptyComputePath == nullptr) {
      this->NonEmptyComputePath = &Path;
      LLVM_DEBUG(llvm::dbgs() << "  Set as NonEmptyComputePath.\n");
    } else {
      // We have to make sure that these two compute path are the same.
      if (!this->NonEmptyComputePath->isIdenticalTo(&Path)) {
        LLVM_DEBUG(llvm::dbgs() << "  Different NonEmptyComputePath.\n");
        this->NonEmptyComputePath = nullptr;
        this->StaticStreamInfo.set_not_stream_reason(
            LLVM::TDG::StaticStreamInfo::MULTI_NON_EMPTY_COMPUTE_PATH);
        break;
      } else {
        LLVM_DEBUG(llvm::dbgs() << "  Identical NonEmptyComputePath.\n");
      }
    }
  }

  /**
   * Set the step pattern by looking at empty compute path.
   */
  if (this->EmptyComputePathFound) {
    this->StaticStreamInfo.set_stp_pattern(
        LLVM::TDG::StreamStepPattern::CONDITIONAL);
  } else {
    this->StaticStreamInfo.set_stp_pattern(
        LLVM::TDG::StreamStepPattern::UNCONDITIONAL);
  }

  if (!this->NonEmptyComputePath) {
    // There is no computation assocated with this PHI node.
    // We do not take it as IndVarStream.
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_NON_EMPTY_COMPUTE_PATH);
    LLVM_DEBUG(llvm::dbgs() << "  Missing NonEmptyComputePath.\n");
    return;
  }
}

void StaticIndVarStream::analyzeValuePattern() {
  // 4a. Check the ValuePattern from the first non-empty SCEV.
  if (!this->NonEmptyComputePath) {
    return;
  }
  const ComputeMetaNode *FirstNonEmptyComputeMNode = nullptr;
  for (const auto &ComputeMNode : this->NonEmptyComputePath->ComputeMetaNodes) {
    if (ComputeMNode->isEmpty()) {
      continue;
    }
    FirstNonEmptyComputeMNode = ComputeMNode;
    break;
  }
  LLVM_DEBUG({
    llvm::dbgs() << "  FirstNonEmpty Value: "
                 << FirstNonEmptyComputeMNode->RootValue->getName()
                 << " SCEV: ";
    if (FirstNonEmptyComputeMNode->SCEV) {
      FirstNonEmptyComputeMNode->SCEV->dump();
    } else {
      llvm::dbgs() << "None\n";
    }
  });
  this->StaticStreamInfo.set_val_pattern(
      this->analyzeValuePatternFromComputePath(FirstNonEmptyComputeMNode));
  LLVM_DEBUG(llvm::dbgs() << "  Value pattern "
                          << ::LLVM::TDG::StreamValuePattern_Name(
                                 this->StaticStreamInfo.val_pattern())
                          << '\n');
}

LLVM::TDG::StreamValuePattern
StaticIndVarStream::analyzeValuePatternFromComputePath(
    const ComputeMetaNode *FirstNonEmptyComputeMNode) {
  assert(this->NonEmptyComputePath && "Missing NonEmptyComputePath.");
  auto SCEV = FirstNonEmptyComputeMNode->SCEV;
  if (SCEV) {
    if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
      if (AddRecSCEV->isAffine()) {

        /**
         * We need to ensure the trip count of each nested loop is LoopInvariant
         */
        auto CurrentLoop = this->InnerMostLoop;
        while (CurrentLoop != this->ConfigureLoop) {
          bool hasConstantTripCount = false;
          auto TripCountSCEV =
              LoopUtils::getTripCountSCEV(this->SE, CurrentLoop);
          if (!llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV) &&
              this->SE->isLoopInvariant(TripCountSCEV, this->ConfigureLoop)) {
            hasConstantTripCount = true;
          }
          if (!hasConstantTripCount) {
            LLVM_DEBUG({
              llvm::dbgs() << "Variant TripCount for " << this->getStreamName()
                           << '\n';
              llvm::dbgs() << " Loop " << LoopUtils::getLoopId(CurrentLoop)
                           << " HasLoopInvariantBackedgeTakeCount "
                           << this->SE->hasLoopInvariantBackedgeTakenCount(
                                  CurrentLoop)
                           << '\n';
              llvm::dbgs() << "TripCountSCEV ";
              TripCountSCEV->print(llvm::dbgs());
              llvm::dbgs() << '\n';
              llvm::dbgs() << "ConstantMaxBackedgeTakenCount ";
              this->SE->getConstantMaxBackedgeTakenCount(CurrentLoop)
                  ->print(llvm::dbgs());
              llvm::dbgs() << '\n';
            });
          }
          if (!hasConstantTripCount) {
            this->StaticStreamInfo.set_not_stream_reason(
                LLVM::TDG::StaticStreamInfo::VARIANT_BACKEDGE_TAKEN);
            return LLVM::TDG::StreamValuePattern::RANDOM;
          }
          CurrentLoop = CurrentLoop->getParentLoop();
        }

        return LLVM::TDG::StreamValuePattern::LINEAR;
      }
    } else if (auto AddSCEV = llvm::dyn_cast<llvm::SCEVAddExpr>(SCEV)) {
      /**
       * If one of the operand is loop invariant, and the other one is our
       * PHINode, then this is LINEAR.
       */
      auto NumOperands = AddSCEV->getNumOperands();
      const llvm::SCEV *LoopVariantSCEV = nullptr;
      bool hasOneLoopVariantSCEV = false;
      for (size_t OperandIdx = 0; OperandIdx < NumOperands; ++OperandIdx) {
        auto OpSCEV = AddSCEV->getOperand(OperandIdx);
        if (this->SE->isLoopInvariant(OpSCEV, this->InnerMostLoop)) {
          // Loop invariant.
          continue;
        }
        if (LoopVariantSCEV == nullptr) {
          LoopVariantSCEV = OpSCEV;
          hasOneLoopVariantSCEV = true;
        } else {
          // More than one variant SCEV. Clear it and break.
          hasOneLoopVariantSCEV = false;
          break;
        }
      }
      // Check if the other one is myself.
      if (hasOneLoopVariantSCEV) {
        assert(this->SE->isSCEVable(this->PHINode->getType()) &&
               "My PHINode should be SCEVable.");
        auto PHINodeSCEV = this->SE->getSCEV(this->PHINode);
        if (PHINodeSCEV == LoopVariantSCEV) {
          return LLVM::TDG::StreamValuePattern::LINEAR;
        }
      }
    }
  }
  if (this->analyzeIsReductionFromComputePath(FirstNonEmptyComputeMNode)) {
    // Let's try to construct the reduction datagraph.
    this->buildReduceDG(FirstNonEmptyComputeMNode);
    if (!this->checkReduceDGComplete()) {
      return LLVM::TDG::StreamValuePattern::RANDOM;
    }
    return LLVM::TDG::StreamValuePattern::REDUCTION;
  } else if (this->analyzeIsPointerChaseFromComputePath(
                 FirstNonEmptyComputeMNode)) {
    // PointerChase also has ReduceDG.
    // For now we don't check if it's complete.
    this->buildReduceDG(FirstNonEmptyComputeMNode);
    return LLVM::TDG::StreamValuePattern::POINTER_CHASE;
  }

  this->StaticStreamInfo.set_not_stream_reason(
      LLVM::TDG::StaticStreamInfo::RANDOM_PATTERN);
  return LLVM::TDG::StreamValuePattern::RANDOM;
}

void StaticIndVarStream::buildReduceDG(
    const ComputeMetaNode *FirstNonEmptyComputeMNode) {
  // The only IndVarStream should be myself.
  auto IsIndVarStream = [this](const llvm::Instruction *Inst) -> bool {
    if (auto PHI = llvm::dyn_cast<llvm::Instruction>(Inst)) {
      if (PHI == this->Inst) {
        return true;
      }
      // Otherwise, search in our IVBaseStream.
      for (auto IVBaseS : this->IndVarBaseStreams) {
        if (IVBaseS->Inst == PHI) {
          return true;
        }
      }
    }
    return false;
  };
  this->ReduceDG = std::make_unique<StreamDataGraph>(
      this->ConfigureLoop, FirstNonEmptyComputeMNode->RootValue,
      IsIndVarStream);
  assert(!this->ReduceDG->hasCircle() && "Circle in ReduceDG.");
}

bool StaticIndVarStream::checkReduceDGComplete() {
  /**
   * This stream has the Reduce pattern. However, we don't try to configure
   * it if we cann't completely remove the computation from the loop, i.e.
   * there are other users within the loop.
   *
   * So far we only allow reduction in one loop level (inner-most loop), but
   * may be configured at outer loop.
   */
  if (StreamPassEnableIncompleteReduction) {
    return true;
  }
  std::unordered_set<const llvm::Instruction *> InstSet = {this->Inst};
  for (auto ComputeInst : this->ReduceDG->getComputeInsts()) {
    InstSet.insert(ComputeInst);
  }
  bool IsComplete = true;
  for (auto Inst : InstSet) {
    for (auto User : Inst->users()) {
      if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
        if (!InstSet.count(UserInst) &&
            this->InnerMostLoop->contains(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "ReduceDG for " << this->getStreamName()
                     << " is incomplete due to user " << UserInst->getName()
                     << " of ComputeInst " << Inst->getName() << '\n');
          IsComplete = false;
          break;
        }
      }
    }
    if (!IsComplete) {
      break;
    }
  }
  /**
   * ! Except the reduction in pr_push.
   * TODO: Really solve this case.
   */
  bool ForceReduction = false;
  if (auto DebugLoc = this->InnerMostLoop->getStartLoc()) {
    auto Scope = llvm::cast<llvm::DIScope>(DebugLoc.getScope());
    auto FileName = Scope->getFilename().str();
    if (FileName.find("pr_push.cc") != std::string::npos) {
      ForceReduction = true;
    }
  }
  if (!IsComplete && !ForceReduction) {
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::IN_LOOP_REDUCTION_USER);
    return false;
  }
  return true;
}

bool StaticIndVarStream::analyzeIsReductionFromComputePath(
    const ComputeMetaNode *FirstNonEmptyComputeMNode) const {

  /**
   * We consider it a reduction stream iff.
   * 1. No EmptyComputePath.
   * 2. Myself as one of the input.
   * 3. Have single or multiple LoadStream input:
   *    a. All the LoadStreams are controlled by the same StepRoot.
   *    b. All the LoadStreams are from the same BB.
   *    c. All the LoadStreams are from the same BB as my computation.
   * 4. Can have the StepRoot as an optional input, no other IVBaseStream.
   */
  if (!StreamPassEnableReduce) {
    return false;
  }
  if (Utils::getDemangledFunctionName(this->Inst->getFunction()) == "sumROI") {
    /**
     * ! Force no reduction in srad_v2 sumROI.
     * This is because so far we only support reduction in the inner most loop
     * level, this this breaks our stream chosen scheme.
     */
    return false;
  }
  LLVM_DEBUG(llvm::dbgs() << "==== Analyze IsReduction "
                          << this->getStreamName() << '\n');
  if (!this->NonEmptyComputePath) {
    LLVM_DEBUG(llvm::dbgs() << "==== [NotReduction] No NonEmpty ComputePath: "
                            << this->AllComputePaths.size() << '\n');
    return false;
  }
  if (this->AllComputePaths.size() != 1) {
    // Multiple compute path.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotReduction] Not Single ComputePath: "
                            << this->AllComputePaths.size() << '\n');
    return false;
  }
  if (this->LoadBaseStreams.empty()) {
    // No LoadStream input to reduce one.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotReduction] No InputLoadS\n");
    return false;
  }
  if (!this->IndVarBaseStreams.count(const_cast<StaticIndVarStream *>(this))) {
    // This is not reduction.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotReduction] Myself Not as Input\n");
    return false;
  }
  // We try to allow ReductionStream configured at outer loop.
  auto FinalInst =
      llvm::dyn_cast<llvm::Instruction>(FirstNonEmptyComputeMNode->RootValue);
  if (!FinalInst) {
    // Final value should be an instruction.
    LLVM_DEBUG(llvm::dbgs()
               << "==== [NotReduction] FinalValue is not Instruction\n");
    return false;
  }
  auto FirstLoadBaseS = *(this->LoadBaseStreams.begin());
  for (auto LoadBaseS : this->LoadBaseStreams) {
    if (LoadBaseS->BaseStepRootStreams != FirstLoadBaseS->BaseStepRootStreams) {
      // Not same StepRoot.
      LLVM_DEBUG(llvm::dbgs()
                 << "==== [NotReduction] Different StepRoot between "
                 << LoadBaseS->getStreamName() << " and "
                 << FirstLoadBaseS->getStreamName() << '\n');
      return false;
    }
    if (LoadBaseS->Inst->getParent() != FinalInst->getParent()) {
      // We are not from the same BB.
      LLVM_DEBUG(llvm::dbgs() << "==== [NotReduction] Different BB between "
                              << LoadBaseS->getStreamName() << " and "
                              << Utils::formatLLVMInst(FinalInst) << '\n');
      return false;
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "==== [IsReduction]\n");
  return true;
}

bool StaticIndVarStream::analyzeIsPointerChaseFromComputePath(
    const ComputeMetaNode *FirstNonEmptyComputeMNode) const {

  /**
   * We consider it a pointer-chase stream iff.
   * 1. No EmptyComputePath.
   * 2. Have single LoadStream input:
   *    a. LoadStream's StepRoot is myself.
   */
  LLVM_DEBUG(llvm::dbgs() << "==== Analyze IsPtrChase " << this->getStreamName()
                          << '\n');
  if (!this->NonEmptyComputePath) {
    return false;
  }
  if (this->AllComputePaths.size() != 1) {
    // Multiple compute path.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] Not Single ComputePath: "
                            << this->AllComputePaths.size() << '\n');
    return false;
  }
  if (this->LoadBaseStreams.size() == 0) {
    // No LoadStream input to reduce one.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] No InputLoadS.\n");
    return false;
  }
  auto FirstLoadBaseS = *(this->LoadBaseStreams.begin());
  for (auto LoadBaseS : this->LoadBaseStreams) {
    if (LoadBaseS->AliasBaseStream != FirstLoadBaseS->AliasBaseStream) {
      LLVM_DEBUG(llvm::dbgs()
                 << "==== [NotPtrChase] InputLoadS Not Same AliasBase: "
                 << LoadBaseS->getStreamName() << ".\n");
      return false;
    }
    if (LoadBaseS->AliasOffset > 64) {
      // Maximum one cache line offset.
      LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] InputLoadS AliasOffset "
                              << LoadBaseS->AliasOffset << " Too Large: "
                              << LoadBaseS->getStreamName() << ".\n");
      return false;
    }
    if (LoadBaseS->BaseStepRootStreams.size() != 1) {
      LLVM_DEBUG({
        llvm::dbgs() << "==== [NotPtrChase] #StepRoot = "
                     << LoadBaseS->BaseStepRootStreams.size()
                     << " for LoadBaseS: " << LoadBaseS->getStreamName()
                     << '\n';
        for (auto BaseStepRootS : LoadBaseS->BaseStepRootStreams) {
          llvm::dbgs() << "   " << BaseStepRootS->getStreamName() << '\n';
        }
      });
      return false;
    }
    if (LoadBaseS->BaseStepRootStreams.count(
            const_cast<StaticIndVarStream *>(this)) == 0) {
      LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] StepRoot not Myself: "
                              << LoadBaseS->getStreamName() << '\n');
      return false;
    }
  }
  if (!this->IndVarBaseStreams.empty()) {
    // This has other IndVarBaseStream, cannot be Pointer Cahse.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] Has IndVarBaseS.\n");
    return false;
  }
  if (this->ConfigureLoop != this->InnerMostLoop) {
    // Only consider inner most loop.
    LLVM_DEBUG(llvm::dbgs() << "==== [NotPtrChase] Configured at Outer Loop\n");
    return false;
  }
  auto FinalInst =
      llvm::dyn_cast<llvm::Instruction>(FirstNonEmptyComputeMNode->RootValue);
  if (!FinalInst) {
    // Final value should be an instruction.
    LLVM_DEBUG(llvm::dbgs()
               << "==== [NotPtrChase] FinalValue is not Instruction\n");
    return false;
  }
  return true;
}

void StaticIndVarStream::analyzeIsCandidate() {
  /**
   * So far only consider inner most loop.
   */
  LLVM_DEBUG(llvm::dbgs() << "====== AnalyzeIsCandidate() - "
                          << this->getStreamName() << '\n');

  if (this->UserNestLoopLevel != UserInvalidNestLoopLevel) {
    auto NestLoopLevel = this->InnerMostLoop->getLoopDepth() -
                         this->ConfigureLoop->getLoopDepth() + 1;
    if (NestLoopLevel != this->UserNestLoopLevel) {
      LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: UserNestLoopLevel "
                              << this->UserNestLoopLevel << " Mine "
                              << NestLoopLevel << '\n');
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::USER_NEST_LOOP_LEVEL);
      this->IsCandidate = false;
      return;
    }
  }

  if (this->UserNoStream) {
    LLVM_DEBUG(llvm::dbgs() << "[NotCandidate]: UserNoStream.\n");
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::USER_NO_STREAM);
    this->IsCandidate = false;
    return;
  }

  if (!this->checkBaseStreamInnerMostLoopContainsMine()) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::
            BASE_STREAM_INNER_MOST_LOOP_NOT_CONTAIN_MINE);
    return;
  }

  /**
   * Start to check constraints:
   * 1. No call inputs.
   * 2. No ComputeMetaNode has more than one PHIMetaNode child.
   * 3. Exactly one "unique" non-empty compute path.
   * 4. In this unique non-empty compute path:
   *   a. The non-empty ComputePath must be a recognizable ValuePattern.
   * 5. Not from remainder/epilogue loop.
   * 6. If this is reduction pattern, make sure that any IndVarBaseS is
   *   the StepRootS of the LoadBaseS. This is separated from analyzeIsReduction
   *   because at that moment, the StepRoot relationship is not built yet.
   */

  // 1.
  if (!this->CallInputs.empty()) {
    this->IsCandidate = false;
    return;
  }

  /**
   * 2-4. Now we just check if we already have the ValuePattern and StepPattern.
   * As they are now collected after constructing MetaGraph.
   */

  if (this->StaticStreamInfo.val_pattern() ==
      LLVM::TDG::StreamValuePattern::RANDOM) {
    // This is not a recognizable pattern.
    this->IsCandidate = false;
    return;
  }

  /**
   * Set the access pattern by looking at empty compute path.
   */
  if (this->StaticStreamInfo.stp_pattern() ==
      LLVM::TDG::StreamStepPattern::CONDITIONAL) {
    if (!StreamPassEnableConditionalStep) {
      this->IsCandidate = false;
      return;
    }
  }

  // 5.
  if (LoopUtils::isLoopRemainderOrEpilogue(this->InnerMostLoop) &&
      !StreamPassEnableLoopRemainderOrEpilogue) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::IN_LOOP_REMAINDER_OR_EPILOGUE);
    return;
  }

  if (this->DependentStreams.empty() &&
      this->StaticStreamInfo.val_pattern() !=
          LLVM::TDG::StreamValuePattern::REDUCTION) {
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::IV_NO_MEM_DEPENDENT);
    return;
  }

  if (this->StaticStreamInfo.val_pattern() ==
      ::LLVM::TDG::StreamValuePattern::REDUCTION) {
    auto FirstLoadBaseS = *(this->LoadBaseStreams.begin());
    // Search for any other possible IVBaseS.
    for (auto IVBaseS : this->IndVarBaseStreams) {
      if (IVBaseS == this) {
        // Ignore myself.
        continue;
      }
      // We only allow additional step root of the LoadStream,
      // as it's likely this is the IV of the loop.
      if (!FirstLoadBaseS->BaseStepRootStreams.count(IVBaseS)) {
        LLVM_DEBUG(llvm::dbgs()
                   << "==== [NotReduction] IVBaseS " << IVBaseS->getStreamName()
                   << " Not StepRootS of " << FirstLoadBaseS->getStreamName()
                   << '\n');
        this->IsCandidate = false;
        this->StaticStreamInfo.set_not_stream_reason(
            ::LLVM::TDG::StaticStreamInfo::MULTI_STEP_ROOT);
        return;
      }
    }
  }

  this->IsCandidate = true;
  return;
}

void StaticIndVarStream::initializeMetaGraphConstruction(
    std::list<DFSNode> &DFSStack,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  this->PHIMetaNodes.emplace_back(this->PHINode);
  ConstructedPHIMetaNodeMap.emplace(this->PHINode, &this->PHIMetaNodes.back());
  this->handleFirstTimePHIMetaNode(DFSStack, &this->PHIMetaNodes.back(),
                                   ConstructedComputeMetaNodeMap);
}

std::list<StaticIndVarStream::ComputePath>
StaticIndVarStream::constructComputePath() const {
  LLVM_DEBUG(llvm::dbgs() << "==== SIVS: ConstructComputePath "
                          << this->getStreamName() << '\n');
  // Start from the root.
  assert(!this->PHIMetaNodes.empty() && "Failed to find the root PHIMetaNode.");
  auto &RootPHIMetaNode = this->PHIMetaNodes.front();
  assert(RootPHIMetaNode.PHINode == this->PHINode &&
         "Mismatched PHINode at root PHIMetaNode.");
  ComputePath CurrentPath;
  std::list<ComputePath> AllComputePaths;
  for (auto &ComputeMNode : RootPHIMetaNode.ComputeMetaNodes) {
    // Remember to ignore the initial value compute node.
    auto RootValue = ComputeMNode->RootValue;
    if (auto RootInst = llvm::dyn_cast<llvm::Instruction>(RootValue)) {
      if (this->InnerMostLoop->contains(RootInst)) {
        // This should not be the ComputeMetaNode for initial value.
        this->constructComputePathRecursive(ComputeMNode, CurrentPath,
                                            AllComputePaths);
      }
    }
  }
  return AllComputePaths;
}

void StaticIndVarStream::constructComputePathRecursive(
    ComputeMetaNode *ComputeMNode, ComputePath &CurrentPath,
    std::list<ComputePath> &AllComputePaths) const {
  assert(ComputeMNode->PHIMetaNodes.size() <= 1 &&
         "Can not handle compute path for more than one PHIMetaNode child.");
  // Add myself to the path.
  LLVM_DEBUG(llvm::dbgs() << "Pushed ComputeMNode " << ComputeMNode->format()
                          << '\n');
  CurrentPath.ComputeMetaNodes.push_back(ComputeMNode);

  if (ComputeMNode->PHIMetaNodes.empty()) {
    // I am the end.
    // This will copy the current path.
    AllComputePaths.push_back(CurrentPath);
  } else {
    // Recursively go to the children.
    auto &PHIMNode = *(ComputeMNode->PHIMetaNodes.begin());
    assert(!PHIMNode->ComputeMetaNodes.empty() &&
           "Every PHIMetaNode should have ComputeMetaNode child.");
    for (auto &ComputeMNodeChild : PHIMNode->ComputeMetaNodes) {
      // This is very hacky, but now let's ignore the compute node already in
      // the path.
      bool AlreadyInPath = false;
      for (const auto &PathComputeMNode : CurrentPath.ComputeMetaNodes) {
        if (PathComputeMNode == ComputeMNodeChild) {
          AlreadyInPath = true;
          break;
        }
      }
      if (!AlreadyInPath) {
        this->constructComputePathRecursive(ComputeMNodeChild, CurrentPath,
                                            AllComputePaths);
      }
    }
  }

  // Remove my self from the path.
  LLVM_DEBUG(llvm::dbgs() << "Poped ComputeMNode "
                          << CurrentPath.ComputeMetaNodes.back()->format()
                          << '\n');
  CurrentPath.ComputeMetaNodes.pop_back();
}

void StaticIndVarStream::ComputePath::debug() const {
  llvm::dbgs() << "ComputePath";
  for (const auto &ComputeMNode : this->ComputeMetaNodes) {
    char Empty = ComputeMNode->isEmpty() ? 'E' : 'F';
    llvm::dbgs() << ' ' << ComputeMNode->format() << '-' << Empty << '-';
    if (!ComputeMNode->isEmpty()) {
      for (const auto &ComputeInst : ComputeMNode->ComputeInsts) {
        llvm::dbgs() << Utils::formatLLVMInstWithoutFunc(ComputeInst) << ',';
      }
    }
    llvm::dbgs() << ' ';
  }
  llvm::dbgs() << '\n';
}

bool StaticIndVarStream::checkIsQualifiedWithoutBackEdgeDep() const {
  if (!this->isCandidate()) {
    return false;
  }
  LLVM_DEBUG(llvm::dbgs() << "[IsQualifed] Without BackEdgeDep for "
                          << this->getStreamName() << '\n');
  // Make sure all the BackBaseStreams have same StepRoot.
  if (this->BackBaseStreams.size() > 1) {
    auto IsSameStepRoot = [](StaticStream *A, StaticStream *B) -> bool {
      if (A->Type == TypeT::IV && B->Type == TypeT::IV) {
        // IV streams are step roots.
        return A == B;
      } else if (A->Type == TypeT::IV) {
        StreamSet StepRootA = {A};
        return StepRootA == B->BaseStepRootStreams;
      } else if (B->Type == TypeT::IV) {
        StreamSet StepRootB = {B};
        return StepRootB == A->BaseStepRootStreams;
      } else {
        return A->BaseStepRootStreams == B->BaseStepRootStreams;
      }
    };
    auto FirstBackBaseS = *(this->BackBaseStreams.begin());
    for (auto BackBaseS : this->BackBaseStreams) {
      if (!IsSameStepRoot(FirstBackBaseS, BackBaseS)) {
        LLVM_DEBUG(llvm::dbgs() << "  [Unqualified] Multiple BackBaseStreams "
                                   "with Different StepRoot.\n");
        return false;
      }
    }
  }
  // Check all the base streams are qualified.
  for (auto &BaseStream : this->BaseStreams) {
    if (!BaseStream->isQualified()) {
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::BASE_STREAM_NOT_QUALIFIED);
      LLVM_DEBUG(llvm::dbgs()
                 << "  [Unqualified] Unqualified BackMemBaseStream "
                 << BaseStream->getStreamName() << ".\n");
      return false;
    }
  }
  // All the base streams are qualified, and thus know their StepPattern. We
  // can check the constraints on static mapping.
  if (!this->checkStaticMapToBaseStreamsInParentLoop()) {
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STATIC_MAPPING);
    LLVM_DEBUG(llvm::dbgs() << "  [Unqualified] No StaticMapping.\n");
    return false;
  }
  LLVM_DEBUG(llvm::dbgs() << "  [Qualified] Without BackEdgeDep.\n");
  return true;
}

bool StaticIndVarStream::checkIsQualifiedWithBackEdgeDep() const {
  for (const auto &BackBaseS : this->BackBaseStreams) {
    if (!BackBaseS->isQualified()) {
      return false;
    }
  }
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

void StaticIndVarStream::finalizePattern() {
  if (!this->BackBaseStreams.empty()) {
    /**
     * We already make sure that all BackBaseStreams have same StepRoot.
     */
    auto BackBaseS = *(this->BackBaseStreams.begin());
    // Check if I am its step root.
    if (BackBaseS->BaseStepRootStreams.count(this) == 0) {
      // I am not its step root, which means this is not a pointer chase.
      if (this->StaticStreamInfo.val_pattern() ==
          LLVM::TDG::StreamValuePattern::REDUCTION) {
        // Do not reset the reduction pattern?
      } else {
        this->StaticStreamInfo.set_val_pattern(
            LLVM::TDG::StreamValuePattern::PREV_LOAD);
      }
    } else {
      this->StaticStreamInfo.set_val_pattern(
          LLVM::TDG::StreamValuePattern::POINTER_CHASE);
    }
  }
  // Only now we compute the StepInst.
  this->searchStepInsts();
}

void StaticIndVarStream::searchStepInsts() {
  std::list<llvm::Instruction *> Queue;

  LLVM_DEBUG(llvm::dbgs() << "Search step instructions for "
                          << this->getStreamName() << '\n');

  /**
   * Special case for reduction stream, which always just use the final compute
   * instruction as the step instruction.
   */
  if (this->StaticStreamInfo.val_pattern() ==
      ::LLVM::TDG::StreamValuePattern::REDUCTION) {
    assert(this->StaticStreamInfo.stp_pattern() ==
           ::LLVM::TDG::StreamStepPattern::UNCONDITIONAL);
    assert(this->NonEmptyComputePath);
    auto FinalInst = llvm::dyn_cast<llvm::Instruction>(
        this->NonEmptyComputePath->ComputeMetaNodes.front()->RootValue);
    LLVM_DEBUG(llvm::dbgs() << "Found step inst "
                            << Utils::formatLLVMInst(FinalInst) << '\n');
    this->StepInsts.insert(FinalInst);
    return;
  }

  for (unsigned IncomingIdx = 0,
                NumIncomingValues = PHINode->getNumIncomingValues();
       IncomingIdx != NumIncomingValues; ++IncomingIdx) {
    auto BB = PHINode->getIncomingBlock(IncomingIdx);
    if (!this->ConfigureLoop->contains(BB)) {
      continue;
    }
    auto IncomingValue = PHINode->getIncomingValue(IncomingIdx);
    if (auto IncomingInst = llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
      Queue.emplace_back(IncomingInst);
      LLVM_DEBUG(llvm::dbgs() << "Enqueue inst "
                              << Utils::formatLLVMInst(IncomingInst) << '\n');
    }
  }

  InstSet VisitedInsts;
  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    LLVM_DEBUG(llvm::dbgs()
               << "Processing " << Utils::formatLLVMInst(CurrentInst) << '\n');
    if (VisitedInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      LLVM_DEBUG(llvm::dbgs() << "Already processed\n");
      continue;
    }
    if (!this->ConfigureLoop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      LLVM_DEBUG(llvm::dbgs() << "Not in loop\n");
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      LLVM_DEBUG(llvm::dbgs() << "Is call or invoke\n");
      continue;
    }

    VisitedInsts.insert(CurrentInst);

    if (StaticStream::isStepInst(CurrentInst)) {
      // Find a step instruction, do not go further.
      LLVM_DEBUG(llvm::dbgs() << "Found step inst "
                              << Utils::formatLLVMInst(CurrentInst) << '\n');
      this->StepInsts.insert(CurrentInst);
    } else if (llvm::isa<llvm::LoadInst>(CurrentInst)) {
      // Base load instruction is also considered as StepInst.
      this->StepInsts.insert(CurrentInst);
    } else {
      // BFS on the operands of non-step instructions.
      for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto OperandValue = CurrentInst->getOperand(OperandIdx);
        if (auto OperandInst =
                llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
          Queue.emplace_back(OperandInst);
          LLVM_DEBUG(llvm::dbgs()
                     << "Enqueue inst " << Utils::formatLLVMInst(OperandInst)
                     << '\n');
        }
      }
    }
  }
}

void StaticIndVarStream::searchComputeInsts() {
  std::list<llvm::Instruction *> Queue;

  LLVM_DEBUG(llvm::dbgs() << "Search compute instructions for "
                          << this->getStreamName() << '\n');

  for (unsigned IncomingIdx = 0,
                NumIncomingValues = this->PHINode->getNumIncomingValues();
       IncomingIdx != NumIncomingValues; ++IncomingIdx) {
    auto BB = this->PHINode->getIncomingBlock(IncomingIdx);
    if (!this->ConfigureLoop->contains(BB)) {
      continue;
    }
    auto IncomingValue = this->PHINode->getIncomingValue(IncomingIdx);
    if (auto IncomingInst = llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
      Queue.emplace_back(IncomingInst);
      LLVM_DEBUG(llvm::dbgs() << "Enqueue inst "
                              << Utils::formatLLVMInst(IncomingInst) << '\n');
    }
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    LLVM_DEBUG(llvm::dbgs()
               << "Processing " << Utils::formatLLVMInst(CurrentInst) << '\n');
    if (this->ComputeInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      LLVM_DEBUG(llvm::dbgs() << "Already processed\n");
      continue;
    }
    if (!this->ConfigureLoop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      LLVM_DEBUG(llvm::dbgs() << "Not in loop\n");
      continue;
    }

    LLVM_DEBUG(llvm::dbgs() << "Found compute inst "
                            << Utils::formatLLVMInst(CurrentInst) << '\n');
    this->ComputeInsts.insert(CurrentInst);

    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(CurrentInst)) {
      // We found a base load. Do not go further.
      continue;
    }

    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        Queue.emplace_back(OperandInst);
        LLVM_DEBUG(llvm::dbgs() << "Enqueue inst "
                                << Utils::formatLLVMInst(OperandInst) << '\n');
      }
    }
  }
}