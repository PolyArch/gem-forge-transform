#include "stream/StaticIndVarStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "StaticIndVarStream"

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
          if (this->SE->hasLoopInvariantBackedgeTakenCount(CurrentLoop)) {
            auto BackEdgeSCEV = this->SE->getBackedgeTakenCount(CurrentLoop);
            if (this->SE->isLoopInvariant(BackEdgeSCEV, this->ConfigureLoop)) {
              hasConstantTripCount = true;
            }
          }
          if (!hasConstantTripCount) {
            LLVM_DEBUG(
                llvm::dbgs()
                    << "Variant BackEdge for " << this->formatName() << '\n';
                llvm::dbgs()
                << " Loop " << LoopUtils::getLoopId(CurrentLoop) << " "
                << this->SE->hasLoopInvariantBackedgeTakenCount(CurrentLoop)
                << '\n';
                this->SE->getBackedgeTakenCount(CurrentLoop)
                    ->print(llvm::dbgs());
                this->SE->getConstantMaxBackedgeTakenCount(CurrentLoop)
                    ->print(llvm::dbgs()));
            llvm::dbgs() << '\n';
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
    // The only IndVarStream should be myself.
    auto IsIndVarStream = [this](const llvm::PHINode *PHI) -> bool {
      if (PHI == this->Inst) {
        return true;
      }
      // Otherwise, search in our IVBaseStream.
      for (auto IVBaseS : this->IndVarBaseStreams) {
        if (IVBaseS->Inst == PHI) {
          return true;
        }
      }
      return false;
    };
    this->ReduceDG = std::make_unique<StreamDataGraph>(
        this->ConfigureLoop, FirstNonEmptyComputeMNode->RootValue,
        IsIndVarStream);
    assert(!this->ReduceDG->hasCircle() && "Circle in ReduceDG.");

    return LLVM::TDG::StreamValuePattern::REDUCTION;
  }

  this->StaticStreamInfo.set_not_stream_reason(
      LLVM::TDG::StaticStreamInfo::RANDOM_PATTERN);
  return LLVM::TDG::StreamValuePattern::RANDOM;
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
   * 4. Can have the StepRoot as an optional input, not no other IVBaseStream.
   */
  if (!StreamPassEnableReduce) {
    return false;
  }
  if (!this->NonEmptyComputePath) {
    return false;
  }
  if (this->AllComputePaths.size() != 1) {
    // Multiple compute path.
    return false;
  }
  if (this->LoadBaseStreams.empty()) {
    // No LoadStream input to reduce one.
    return false;
  }
  if (!this->IndVarBaseStreams.count(const_cast<StaticIndVarStream *>(this))) {
    // This is not reduction.
    return false;
  }
  if (this->ConfigureLoop != this->InnerMostLoop) {
    // Only consider inner most loop.
    return false;
  }
  auto FinalInst =
      llvm::dyn_cast<llvm::Instruction>(FirstNonEmptyComputeMNode->RootValue);
  if (!FinalInst) {
    // Final value should be an instruction.
    return false;
  }
  auto FirstLoadBaseS = *(this->LoadBaseStreams.begin());
  for (auto LoadBaseS : this->LoadBaseStreams) {
    if (LoadBaseS->BaseStepRootStreams != FirstLoadBaseS->BaseStepRootStreams) {
      // Not same StepRoot.
      return false;
    }
    if (LoadBaseS->Inst->getParent() != FinalInst->getParent()) {
      // We are not from the same BB.
      return false;
    }
  }
  // Search for any other possible IVBaseS.
  for (auto IVBaseS : this->IndVarBaseStreams) {
    if (IVBaseS == this) {
      // Ignore myself.
      continue;
    }
    // We only allow additional step root of the LoadStream,
    // as it's likely this is the IV of the loop.
    if (!FirstLoadBaseS->BaseStepRootStreams.count(IVBaseS)) {
      return false;
    }
  }

  return true;
}

void StaticIndVarStream::analyzeIsCandidate() {
  /**
   * So far only consider inner most loop.
   */
  LLVM_DEBUG(llvm::dbgs() << "====== AnalyzeIsCandidate() - "
                          << this->formatName() << '\n');

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
   */

  // 1.
  if (!this->CallInputs.empty()) {
    this->IsCandidate = false;
    return;
  }

  // 2.
  for (const auto &ComputeMNode : this->ComputeMetaNodes) {
    if (ComputeMNode.PHIMetaNodes.size() > 1) {
      this->IsCandidate = false;
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::MULTI_PHIMNODE_FOR_COMPUTEMNODE);
      return;
    }
  }

  // 3.
  this->AllComputePaths = this->constructComputePath();

  auto EmptyPathFound = false;
  LLVM_DEBUG(llvm::dbgs() << "Analyzing ComputePath of " << this->formatName()
                          << '\n');
  for (const auto &Path : AllComputePaths) {
    LLVM_DEBUG(Path.debug());
    if (Path.isEmpty()) {
      EmptyPathFound = true;
      continue;
    }
    if (this->NonEmptyComputePath == nullptr) {
      this->NonEmptyComputePath = &Path;
      LLVM_DEBUG(llvm::dbgs() << "Set as NonEmptyComputePath.\n");
    } else {
      // We have to make sure that these two compute path are the same.
      if (!this->NonEmptyComputePath->isIdenticalTo(&Path)) {
        LLVM_DEBUG(llvm::dbgs() << "Different NonEmptyComputePath.\n");
        this->NonEmptyComputePath = nullptr;
        this->IsCandidate = false;
        this->StaticStreamInfo.set_not_stream_reason(
            LLVM::TDG::StaticStreamInfo::MULTI_NON_EMPTY_COMPUTE_PATH);
        return;
      } else {
        LLVM_DEBUG(llvm::dbgs() << "Identical NonEmptyComputePath.\n");
      }
    }
  }

  if (!this->NonEmptyComputePath) {
    // There is no computation assocated with this PHI node.
    // We do not take it as IndVarStream.
    this->IsCandidate = false;
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_NON_EMPTY_COMPUTE_PATH);
    LLVM_DEBUG(llvm::dbgs() << "Missing NonEmptyComputePath.\n");
    return;
  }

  // 4.
  // for (const auto &ComputeMNode :
  //      this->NonEmptyComputePath->ComputeMetaNodes) {
  //   // 4a. No empty SCEV.
  //   if (ComputeMNode->SCEV == nullptr) {
  //     this->IsCandidate = false;
  //     this->StaticStreamInfo.set_not_stream_reason(
  //         LLVM::TDG::StaticStreamInfo::NOT_SCEVABLE_COMPUTEMNODE);
  //     return;
  //   }
  // }
  // 4b. Check the ValuePattern from the first non-empty SCEV.
  const ComputeMetaNode *FirstNonEmptyComputeMNode = nullptr;
  for (const auto &ComputeMNode : this->NonEmptyComputePath->ComputeMetaNodes) {
    if (ComputeMNode->isEmpty()) {
      continue;
    }
    FirstNonEmptyComputeMNode = ComputeMNode;
    break;
  }
  LLVM_DEBUG(llvm::dbgs() << "StaticIVStream: FirstNonEmpty Value: "
                          << FirstNonEmptyComputeMNode->RootValue->getName()
                          << " SCEV: ";
             if (FirstNonEmptyComputeMNode->SCEV)
                 FirstNonEmptyComputeMNode->SCEV->dump();
             else llvm::dbgs() << "None\n");
  this->StaticStreamInfo.set_val_pattern(
      this->analyzeValuePatternFromComputePath(FirstNonEmptyComputeMNode));
  LLVM_DEBUG(llvm::dbgs() << this->formatName() << ": Value pattern "
                          << ::LLVM::TDG::StreamValuePattern_Name(
                                 this->StaticStreamInfo.val_pattern())
                          << '\n');

  if (this->StaticStreamInfo.val_pattern() ==
      LLVM::TDG::StreamValuePattern::RANDOM) {
    // This is not a recognizable pattern.
    this->IsCandidate = false;
    return;
  }

  /**
   * Set the access pattern by looking at empty compute path.
   */
  if (EmptyPathFound) {
    this->StaticStreamInfo.set_stp_pattern(
        LLVM::TDG::StreamStepPattern::CONDITIONAL);
    if (!StreamPassEnableConditionalStep) {
      this->IsCandidate = false;
      return;
    }
  } else {
    this->StaticStreamInfo.set_stp_pattern(
        LLVM::TDG::StreamStepPattern::UNCONDITIONAL);
  }

  // 5.
  if (LoopUtils::isLoopRemainderOrEpilogue(this->InnerMostLoop)) {
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
                          << this->formatName() << '\n');
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
  LLVM_DEBUG(llvm::dbgs() << "Check IsQualifed Without BackEdgeDep for "
                          << this->formatName() << '\n');
  // Make sure all the BackMemBaseStreams have same StepRoot.
  if (this->BackMemBaseStreams.size() > 1) {
    auto FirstBackMemBaseStream = *(this->BackMemBaseStreams.begin());
    for (auto BackMemBaseS : this->BackMemBaseStreams) {
      if (BackMemBaseS->BaseStepRootStreams !=
          FirstBackMemBaseStream->BaseStepRootStreams) {
        LLVM_DEBUG(llvm::dbgs() << "[Unqualified] Multiple BackMemBaseStreams "
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
      LLVM_DEBUG(llvm::dbgs() << "[Unqualified] Unqualified BackMemBaseStream "
                              << BaseStream->formatName() << ".\n");
      return false;
    }
  }
  // All the base streams are qualified, and thus know their StepPattern. We
  // can check the constraints on static mapping.
  if (!this->checkStaticMapFromBaseStreamInParentLoop()) {
    this->StaticStreamInfo.set_not_stream_reason(
        LLVM::TDG::StaticStreamInfo::NO_STATIC_MAPPING);
    LLVM_DEBUG(llvm::dbgs() << "[Unqualified] No StaticMapping.\n");
    return false;
  }
  return true;
}

bool StaticIndVarStream::checkIsQualifiedWithBackEdgeDep() const {
  for (const auto &BackMemBaseStream : this->BackMemBaseStreams) {
    if (!BackMemBaseStream->isQualified()) {
      return false;
    }
  }
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

void StaticIndVarStream::finalizePattern() {
  if (!this->BackMemBaseStreams.empty()) {
    /**
     * We already make sure that all BackMemBaseStreams have same StepRoot.
     */
    auto BackMemBaseStream = *(this->BackMemBaseStreams.begin());
    // Check if I am its step root.
    if (BackMemBaseStream->BaseStepRootStreams.count(this) == 0) {
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
                          << this->formatName() << '\n');

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
                          << this->formatName() << '\n');

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