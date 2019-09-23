#include "stream/StaticIndVarStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"

LLVM::TDG::StreamValuePattern
StaticIndVarStream::analyzeValuePatternFromSCEV(const llvm::SCEV *SCEV) const {

  llvm::errs() << (SCEV == nullptr) << '\n';
  if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
    if (AddRecSCEV->isAffine()) {

      return LLVM::TDG::StreamValuePattern::LINEAR;
    }
  } else if (auto AddSCEV = llvm::dyn_cast<llvm::SCEVAddExpr>(SCEV)) {
    /**
     * If one of the operand is loop invariant, and the other one is our
     * PHINode, then this is LINEAR.
     */
    auto NumOperands = AddSCEV->getNumOperands();
    const llvm::SCEV *LoopVariantSCEV = nullptr;
    for (size_t OperandIdx = 0; OperandIdx < NumOperands; ++OperandIdx) {
      auto OpSCEV = AddSCEV->getOperand(OperandIdx);
      if (this->SE->isLoopInvariant(OpSCEV, this->InnerMostLoop)) {
        // Loop invariant.
        continue;
      }
      if (LoopVariantSCEV == nullptr) {
        LoopVariantSCEV = OpSCEV;
      } else {
        // More than one variant SCEV.
        return LLVM::TDG::StreamValuePattern::RANDOM;
      }
    }
    // Check if the other one is myself.
    assert(this->SE->isSCEVable(this->PHINode->getType()) &&
           "My PHINode should be SCEVable.");
    auto PHINodeSCEV = this->SE->getSCEV(this->PHINode);
    if (PHINodeSCEV == LoopVariantSCEV) {
      return LLVM::TDG::StreamValuePattern::LINEAR;
    }
  }

  return LLVM::TDG::StreamValuePattern::RANDOM;
}

bool StaticIndVarStream::ComputeMetaNode::isIdenticalTo(
    const ComputeMetaNode *Other) const {
  /**
   * Check if I am the same as the other compute node.
   */
  if (Other == this) {
    return true;
  }
  if (Other->ComputeInsts.size() != this->ComputeInsts.size()) {
    return false;
  }
  for (size_t ComputeInstIdx = 0, NumComputeInsts = this->ComputeInsts.size();
       ComputeInstIdx != NumComputeInsts; ++ComputeInstIdx) {
    const auto &ThisComputeInst = this->ComputeInsts.at(ComputeInstIdx);
    const auto &OtherComputeInst = Other->ComputeInsts.at(ComputeInstIdx);
    if (ThisComputeInst->getOpcode() != OtherComputeInst->getOpcode()) {
      return false;
    }
  }
  // We need to be more careful to check the inputs.
  if (this->LoadBaseStreams != Other->LoadBaseStreams) {
    return false;
  }
  if (this->CallInputs != Other->CallInputs) {
    return false;
  }
  if (this->LoopInvariantInputs != Other->LoopInvariantInputs) {
    return false;
  }
  if (this->IndVarBaseStreams != Other->IndVarBaseStreams) {
    return false;
  }

  return true;
}

void StaticIndVarStream::analyzeIsCandidate() {
  /**
   * So far only consider inner most loop.
   */
  llvm::errs() << this->formatName() << '\n';

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
   * 3. At most one "unique" non-empty compute path.
   * 4. In this unique non-empty compute path:
   *   a. All non-empty ComputeMNode has the SCEV.
   *   b. The first SCEV must be a recognizable ValuePattern.
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
  for (const auto &Path : AllComputePaths) {
    if (Path.isEmpty()) {
      EmptyPathFound = true;
      continue;
    }
    if (this->NonEmptyComputePath == nullptr) {
      this->NonEmptyComputePath = &Path;
    } else {
      // We have to make sure that these two compute path are the same.
      if (!this->NonEmptyComputePath->isIdenticalTo(&Path)) {
        this->IsCandidate = false;
        this->StaticStreamInfo.set_not_stream_reason(
            LLVM::TDG::StaticStreamInfo::MULTI_NON_EMPTY_COMPUTE_PATH);
        return;
      }
    }
  }

  // 4.
  if (this->NonEmptyComputePath != nullptr) {
    for (const auto &ComputeMNode :
         this->NonEmptyComputePath->ComputeMetaNodes) {
      // 4a. No empty SCEV.
      if (ComputeMNode->SCEV == nullptr) {
        this->IsCandidate = false;
        this->StaticStreamInfo.set_not_stream_reason(
            LLVM::TDG::StaticStreamInfo::NOT_SCEVABLE_COMPUTEMNODE);
        return;
      }
    }
    // 4b. Check the ValuePattern from the first non-empty SCEV.
    const ComputeMetaNode *FirstNonEmptyComputeMNode = nullptr;
    for (const auto &ComputeMNode :
         this->NonEmptyComputePath->ComputeMetaNodes) {
      if (ComputeMNode->isEmpty()) {
        continue;
      }
      FirstNonEmptyComputeMNode = ComputeMNode;
      break;
    }
    this->StaticStreamInfo.set_val_pattern(
        this->analyzeValuePatternFromSCEV(FirstNonEmptyComputeMNode->SCEV));
    if (this->StaticStreamInfo.val_pattern() ==
        LLVM::TDG::StreamValuePattern::RANDOM) {
      // This is not a recognizable pattern.
      this->IsCandidate = false;
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::RANDOM_PATTERN);
      return;
    }

    /**
     * Set the access pattern by looking at empty compute path.
     */
    if (EmptyPathFound) {
      this->StaticStreamInfo.set_stp_pattern(
          LLVM::TDG::StreamStepPattern::CONDITIONAL);
    } else {
      this->StaticStreamInfo.set_stp_pattern(
          LLVM::TDG::StreamStepPattern::UNCONDITIONAL);
    }
    this->IsCandidate = true;

    /**
     * If this is LINEAR pattern, we generate the parameters.
     */
    if (this->StaticStreamInfo.val_pattern() ==
        LLVM::TDG::StreamValuePattern::LINEAR) {
      // Careful, we should use SCEV for ourselve to get correct start value.
      auto PHINodeSCEV = this->SE->getSCEV(this->PHINode);
      // So far we only handle AddRec.
      // TODO: Handle Add.
      if (auto PHINodeAddRecSCEV =
              llvm::dyn_cast<llvm::SCEVAddRecExpr>(PHINodeSCEV)) {
        this->InputValuesValid = true;
        auto ProtoIVPattern = this->StaticStreamInfo.mutable_iv_pattern();
        ProtoIVPattern->set_val_pattern(LLVM::TDG::StreamValuePattern::LINEAR);

        /**
         * Linear pattern has two parameters, base and stride.
         * TODO: Handle non-constant params in the future.
         */
        auto BaseSCEV = PHINodeAddRecSCEV->getStart();
        this->addInputParam(BaseSCEV, false);
        auto StrideSCEV = PHINodeAddRecSCEV->getOperand(1);
        this->addInputParam(StrideSCEV, true);
      }
    }

    return;
  }

  llvm::errs() << "Done\n";

  this->IsCandidate = true;
}

void StaticIndVarStream::addInputParam(const llvm::SCEV *SCEV, bool Signed) {

  auto ProtoIVPattern = this->StaticStreamInfo.mutable_iv_pattern();
  auto ProtoParam = ProtoIVPattern->add_params();
  if (auto ConstSCEV = llvm::dyn_cast<llvm::SCEVConstant>(SCEV)) {
    ProtoParam->set_valid(true);
    if (Signed)
      ProtoParam->set_param(ConstSCEV->getValue()->getSExtValue());
    else
      ProtoParam->set_param(ConstSCEV->getValue()->getZExtValue());
  } else if (auto UnknownSCEV = llvm::dyn_cast<llvm::SCEVUnknown>(SCEV)) {
    ProtoParam->set_valid(false);
    this->InputValues.push_back(UnknownSCEV->getValue());
  } else {
    // Search through the child compute nodes.
    const auto &PHIMNode = this->PHIMetaNodes.front();
    for (const auto &ComputeMNode : PHIMNode.ComputeMetaNodes) {
      if (ComputeMNode->SCEV == SCEV) {
        ProtoParam->set_valid(false);
        // We found the SCEV, check if the ComputeMetaNode is empty.
        if (ComputeMNode->isEmpty()) {
          this->InputValues.push_back(ComputeMNode->RootValue);
          return;
        }
      }
    }
    SCEV->print(llvm::errs());
    assert(false && "Cannot handle this SCEV so far.");
  }
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
  CurrentPath.ComputeMetaNodes.pop_back();
}

void StaticIndVarStream::ComputePath::debug() const {
  llvm::errs() << "ComputePath ";
  for (const auto &ComputeMNode : this->ComputeMetaNodes) {
    char Empty = ComputeMNode->isEmpty() ? 'E' : 'F';
    llvm::errs() << Utils::formatLLVMValueWithoutFunc(ComputeMNode->RootValue)
                 << Empty;
    if (!ComputeMNode->isEmpty()) {
      for (const auto &ComputeInst : ComputeMNode->ComputeInsts) {
        llvm::errs() << Utils::formatLLVMInstWithoutFunc(ComputeInst) << ',';
      }
    }
    llvm::errs() << ' ';
  }
  llvm::errs() << '\n';
}

bool StaticIndVarStream::checkIsQualifiedWithoutBackEdgeDep() const {
  if (!this->isCandidate()) {
    return false;
  }
  // Make sure we only has one back edge.
  if (this->BackMemBaseStreams.size() > 1) {
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

bool StaticIndVarStream::checkIsQualifiedWithBackEdgeDep() const {
  for (const auto &BackMemBaseStream : this->BackMemBaseStreams) {
    if (!BackMemBaseStream->isQualified()) {
      return false;
    }
  }
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

void StaticIndVarStream::finalizePattern() {
  assert(this->BackMemBaseStreams.size() <= 1 &&
         "More than 1 back edge dependencies.");
  if (this->BackMemBaseStreams.empty()) {
    // Without back edge dependency, the pattern is already computed.
    return;
  }
  auto BackMemBaseStream = *(this->BackMemBaseStreams.begin());
  // Check if I am its step root.
  if (BackMemBaseStream->BaseStepRootStreams.count(this) == 0) {
    // I am not its step root, which means this is not a pointer chase.
    this->StaticStreamInfo.set_val_pattern(
        LLVM::TDG::StreamValuePattern::PREV_LOAD);
  } else {
    this->StaticStreamInfo.set_val_pattern(
        LLVM::TDG::StreamValuePattern::POINTER_CHASE);
  }
}