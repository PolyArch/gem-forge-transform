#include "stream/StaticStream.h"

#include "llvm/Support/raw_ostream.h"

void StaticStream::setStaticStreamInfo(LLVM::TDG::StaticStreamInfo &SSI) const {
  SSI.set_is_stream(this->IsStream);
  SSI.set_acc_pattern(this->AccPattern);
  SSI.set_val_pattern(this->ValPattern);
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

void StaticIndVarStream::analyze() {
  /**
   * Start to check constraints:
   * 1. No call inputs.
   * 2. No ComputeMetaNode has more than one PHIMetaNode child.
   * 3. At most one "unique" non-empty compute path.
   * 4. In this unique non-empty compute path:
   *   a. All non-empty ComputeMNode has the SCEV.
   *   b.
   */

  // 1.
  if (!this->CallInputs.empty()) {
    this->IsStream = false;
    return;
  }

  // 2.
  for (const auto &ComputeMNode : this->ComputeMetaNodes) {
    if (ComputeMNode.PHIMetaNodes.size() > 1) {
      this->IsStream = false;
      return;
    }
  }

  // 3.
  auto AllComputePaths = this->constructComputePath();
  const ComputePath *CurrentNonEmptyComputePath = nullptr;
  auto EmptyPathFound = false;
  for (const auto &Path : AllComputePaths) {
    if (Path.isEmpty()) {
      EmptyPathFound = true;
      continue;
    }
    if (CurrentNonEmptyComputePath == nullptr) {
      CurrentNonEmptyComputePath = &Path;
    } else {
      // We have to make sure that these two compute path are the same.
      if (!CurrentNonEmptyComputePath->isIdenticalTo(&Path)) {
        this->IsStream = false;
        return;
      }
    }
  }

  // 4. Only add,sub are allowed so far. TODO.
  if (CurrentNonEmptyComputePath != nullptr) {
    for (const auto &ComputeMNode :
         CurrentNonEmptyComputePath->ComputeMetaNodes) {
      if (ComputeMNode->SCEV == nullptr) {
        this->IsStream = false;
        return;
      }
    }
  }

  /**
   * Set the access pattern by looking at empty compute path.
   */
  if (EmptyPathFound) {
    this->AccPattern = LLVM::TDG::StreamAccessPattern::CONDITIONAL;
  } else {
    this->AccPattern = LLVM::TDG::StreamAccessPattern::UNCONDITIONAL;
  }
  this->IsStream = true;
  return;
}

void StaticIndVarStream::handleFirstTimeComputeNode(
    std::list<DFSNode> &DFSStack, DFSNode &DNode,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  auto ComputeMNode = DNode.ComputeMNode;

  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(DNode.Value)) {
    if (this->InnerMostLoop->contains(Inst)) {
      if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
        // Load input.
        ComputeMNode->LoadInputs.insert(LoadInst);
        this->LoadInputs.insert(LoadInst);
        DFSStack.pop_back();
      } else if (llvm::isa<llvm::PHINode>(Inst) &&
                 this->InnerMostLoop->getHeader() == Inst->getParent()) {
        // Loop header phi inputs.
        auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst);
        ComputeMNode->LoopHeaderPHIInputs.insert(PHINode);
        this->LoopHeaderPHIInputs.insert(PHINode);
        DFSStack.pop_back();
      } else if (Utils::isCallOrInvokeInst(Inst)) {
        // Call input.
        ComputeMNode->CallInputs.insert(Inst);
        this->CallInputs.insert(Inst);
        DFSStack.pop_back();
      } else if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        // Another PHIMeta Node.
        if (ConstructedPHIMetaNodeMap.count(PHINode) == 0) {
          // First time encounter the phi node.
          this->PHIMetaNodes.emplace_back(PHINode);
          ConstructedPHIMetaNodeMap.emplace(PHINode,
                                            &this->PHIMetaNodes.back());
          this->handleFirstTimePHIMetaNode(DFSStack, &this->PHIMetaNodes.back(),
                                           ConstructedComputeMetaNodeMap);
        }
        auto PHIMNode = ConstructedPHIMetaNodeMap.at(PHINode);
        ComputeMNode->PHIMetaNodes.insert(PHIMNode);
        DFSStack.pop_back();
      } else {
        // Normal compute inst.
        for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
             OperandIdx != NumOperands; ++OperandIdx) {
          DFSStack.emplace_back(ComputeMNode, Inst->getOperand(OperandIdx));
        }
      }
    } else {
      // Loop Invarient Inst.
      ComputeMNode->LoopInvarianteInputs.insert(Inst);
      this->LoopInvarianteInputs.insert(Inst);
      DFSStack.pop_back();
    }
  } else {
    // Loop Invarient Value.
    ComputeMNode->LoopInvarianteInputs.insert(DNode.Value);
    this->LoopInvarianteInputs.insert(DNode.Value);
    DFSStack.pop_back();
  }
}

void StaticIndVarStream::handleFirstTimePHIMetaNode(
    std::list<DFSNode> &DFSStack, PHIMetaNode *PHIMNode,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  auto PHINode = PHIMNode->PHINode;
  for (unsigned IncomingIdx = 0,
                NumIncomingValues = PHINode->getNumIncomingValues();
       IncomingIdx != NumIncomingValues; ++IncomingIdx) {
    auto BB = PHINode->getIncomingBlock(IncomingIdx);
    auto IncomingValue = PHINode->getIncomingValue(IncomingIdx);
    // Check if there is a constructed compute node.
    if (ConstructedComputeMetaNodeMap.count(IncomingValue) == 0) {
      // First time.
      const llvm::SCEV *SCEV = nullptr;
      if (this->SE->isSCEVable(IncomingValue->getType())) {
        SCEV = this->SE->getSCEV(IncomingValue);
      }
      this->ComputeMetaNodes.emplace_back(IncomingValue, SCEV);
      ConstructedComputeMetaNodeMap.emplace(IncomingValue,
                                            &this->ComputeMetaNodes.back());
      // Push to the stack.
      DFSStack.emplace_back(&this->ComputeMetaNodes.back(), IncomingValue);
    }
    // Add to my children.
    PHIMNode->ComputeMetaNodes.insert(
        ConstructedComputeMetaNodeMap.at(IncomingValue));
  }
}

void StaticIndVarStream::constructMetaGraph() {
  // Create the first PHIMetaNode.
  ConstructedPHIMetaNodeMapT ConstructedPHIMetaNodeMap;
  ConstructedComputeMetaNodeMapT ConstructedComputeMetaNodeMap;
  this->PHIMetaNodes.emplace_back(this->PHINode);
  ConstructedPHIMetaNodeMap.emplace(this->PHINode, &this->PHIMetaNodes.back());
  std::list<DFSNode> DFSStack;
  this->handleFirstTimePHIMetaNode(DFSStack, &this->PHIMetaNodes.back(),
                                   ConstructedComputeMetaNodeMap);
  while (!DFSStack.empty()) {
    auto &DNode = DFSStack.back();
    if (DNode.VisitTimes == 0) {
      // First time.
      DNode.VisitTimes++;
      this->handleFirstTimeComputeNode(DFSStack, DNode,
                                       ConstructedPHIMetaNodeMap,
                                       ConstructedComputeMetaNodeMap);
    } else if (DNode.VisitTimes == 1) {
      // Second time. This must be a real compute inst.
      bool Inserted = false;
      auto Inst = llvm::dyn_cast<llvm::Instruction>(DNode.Value);
      assert(Inst && "Only compute inst will be handled twice.");
      for (auto ComputeInst : DNode.ComputeMNode->ComputeInsts) {
        if (ComputeInst == Inst) {
          Inserted = true;
          break;
        }
      }
      if (!Inserted) {
        // Add my self to the compute insts.
        // This will ensure the topological order.
        DNode.ComputeMNode->ComputeInsts.push_back(Inst);
      }
      DFSStack.pop_back();
    }
  }
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
}