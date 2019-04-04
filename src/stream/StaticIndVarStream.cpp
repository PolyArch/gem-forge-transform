#include "stream/StaticIndVarStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"

LLVM::TDG::StreamValuePattern
StaticIndVarStream::analyzeValuePatternFromSCEV(const llvm::SCEV *SCEV) const {
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
    assert(NumOperands == 2 && "AddSCEV should have 2 operands.");
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
  if (this->LoadInputs != Other->LoadInputs) {
    return false;
  }
  if (this->CallInputs != Other->CallInputs) {
    return false;
  }
  if (this->LoopInvarianteInputs != Other->LoopInvarianteInputs) {
    return false;
  }
  if (this->LoopHeaderPHIInputs != Other->LoopHeaderPHIInputs) {
    return false;
  }

  return true;
}

void StaticIndVarStream::analyzeIsCandidate() {
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
    this->ValPattern =
        this->analyzeValuePatternFromSCEV(FirstNonEmptyComputeMNode->SCEV);
    if (this->ValPattern == LLVM::TDG::StreamValuePattern::RANDOM) {
      // This is not a recognizable pattern.
      this->IsCandidate = false;
      return;
    }

    /**
     * Set the access pattern by looking at empty compute path.
     */
    if (EmptyPathFound) {
      this->AccPattern = LLVM::TDG::StreamAccessPattern::CONDITIONAL;
    } else {
      this->AccPattern = LLVM::TDG::StreamAccessPattern::UNCONDITIONAL;
    }
    this->IsCandidate = true;
    return;
  }

  this->IsCandidate = true;
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
      this->constructComputePathRecursive(ComputeMNodeChild, CurrentPath,
                                          AllComputePaths);
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

void StaticIndVarStream::buildDependenceGraph(GetStreamFuncT GetStream) {
  for (const auto &LoadInput : this->LoadInputs) {
    auto BaseMStream = GetStream(LoadInput, this->ConfigureLoop);
    assert(BaseMStream != nullptr && "Failed to get back-edge MStream.");
    this->addBackEdgeBaseStream(BaseMStream);
  }
}