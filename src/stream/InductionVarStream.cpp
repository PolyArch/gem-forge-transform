#include "stream/InductionVarStream.h"

#include <list>

#define DEBUG_TYPE "InductionVarStream"

InductionVarStream::InductionVarStream(const std::string &_Folder,
                                       const std::string &_RelativeFolder,
                                       const llvm::PHINode *_PHIInst,
                                       const llvm::Loop *_Loop,
                                       const llvm::Loop *_InnerMostLoop,
                                       size_t _Level,
                                       llvm::DataLayout *DataLayout)
    : Stream(TypeT::IV, _Folder, _RelativeFolder, _PHIInst, _Loop,
             _InnerMostLoop, _Level, DataLayout),
      PHIInst(_PHIInst) {
  this->searchComputeInsts(this->PHIInst, this->Loop);
  this->StepInsts =
      InductionVarStream::searchStepInsts(this->PHIInst, this->InnerMostLoop);
  this->IsCandidateStatic = this->isCandidateStatic();
  this->IsStaticStream = this->isStaticStream();
}

bool InductionVarStream::isCandidateStatic() const {
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
      if (this->InnerMostLoop->contains(BaseLoad)) {
        BaseLoadInTheInnerMostLoop++;
      }
    }
    if (BaseLoadInTheInnerMostLoop > 1) {
      return false;
    }
    // For iv stream with base loads, we only allow it to be configured at inner
    // most loop level.
    if (BaseLoadInTheInnerMostLoop > 0 && this->Loop != this->InnerMostLoop) {
      return false;
    }

  } else {
    if (!this->BaseLoadInsts.empty()) {
      return false;
    }
  }
  return true;
}

void InductionVarStream::searchComputeInsts(const llvm::PHINode *PHINode,
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

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    DEBUG(llvm::errs() << "Processing "
                       << LoopUtils::formatLLVMInst(CurrentInst) << '\n');
    if (this->ComputeInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      DEBUG(llvm::errs() << "Already processed\n");
      continue;
    }
    if (!Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      DEBUG(llvm::errs() << "Not in loop\n");
      continue;
    }

    DEBUG(llvm::errs() << "Found compute inst "
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
        DEBUG(llvm::errs() << "Enqueue inst "
                           << LoopUtils::formatLLVMInst(OperandInst) << '\n');
      }
    }
  }
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
          DEBUG(llvm::errs() << "Enqueue inst "
                             << LoopUtils::formatLLVMInst(OperandInst) << '\n');
        }
      }
    }
  }
  return StepInsts;
}

void InductionVarStream::buildBasicDependenceGraph(GetStreamFuncT GetStream) {
  // Check the back edge.
  for (const auto &BaseLoad : this->BaseLoadInsts) {
    auto BaseMStream = GetStream(BaseLoad, this->Loop);
    assert(BaseMStream != nullptr && "Failed to get back-edge MStream.");
    this->addBackEdgeBaseStream(BaseMStream);
  }
}

bool InductionVarStream::isCandidate() const { return this->IsCandidateStatic; }

bool InductionVarStream::isQualifySeed() const {
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

void InductionVarStream::buildChosenDependenceGraph(
    GetChosenStreamFuncT GetChosenStream) {
  // Check the back edge.
  for (const auto &BaseLoad : this->BaseLoadInsts) {
    auto BaseMStream = GetChosenStream(BaseLoad);
    assert(BaseMStream != nullptr && "Failed to get chosen back-edge MStream.");
    this->addChosenBackEdgeBaseStream(BaseMStream);
  }
}

namespace {
class StaticIVStreamAnalyzer {
 public:
  StaticIVStreamAnalyzer(const llvm::Loop *_InnerMostLoop,
                         const llvm::PHINode *_PHINode)
      : InnerMostLoop(_InnerMostLoop), PHINode(_PHINode) {
    this->constructMetaGraph();
  }

  struct MetaNode {
    enum TypeE {
      PHIMetaNodeEnum,
      ComputeMetaNodeEnum,
    };
    TypeE Type;
  };

  struct ComputeMetaNode;
  struct PHIMetaNode : public MetaNode {
    PHIMetaNode(const llvm::PHINode *_PHINode) : PHINode(_PHINode) {
      this->Type = PHIMetaNodeEnum;
    }
    std::unordered_set<ComputeMetaNode *> ComputeMetaNodes;
    const llvm::PHINode *PHINode;
  };

  struct ComputeMetaNode : public MetaNode {
    ComputeMetaNode() { this->Type = ComputeMetaNodeEnum; }
    std::unordered_set<PHIMetaNode *> PHIMetaNodes;
    std::unordered_set<const llvm::LoadInst *> LoadInputs;
    std::unordered_set<const llvm::Instruction *> CallInputs;
    std::unordered_set<const llvm::Value *> LoopInvarianteInputs;
    std::unordered_set<const llvm::PHINode *> LoopHeaderPHIInputs;
    std::vector<const llvm::Instruction *> ComputeInsts;
    bool isEmpty() const {
      /**
       * Check if this ComputeMNode does nothing.
       * So far we just check that there is no compute insts.
       * Further we can allow something like binary extension.
       */
      return this->ComputeInsts.empty();
    }
    bool isIdenticalTo(const ComputeMetaNode *Other) const {
      /**
       * Check if I am the same as the other compute node.
       */
      if (Other == this) {
        return true;
      }
      if (Other->ComputeInsts.size() != this->ComputeInsts.size()) {
        return false;
      }
      for (size_t ComputeInstIdx = 0,
                  NumComputeInsts = this->ComputeInsts.size();
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
  };

  bool isStaticStream() {
    /**
     * Start to check constraints:
     * 1. No call inputs.
     * 2. No ComputeMetaNode has more than one PHIMetaNode child.
     * 3. At most one "unique" non-empty compute path.
     * 4. In this unique non-empty compute path, only special instructions are
     * allowed.
     */

    // 1.
    if (!this->CallInputs.empty()) {
      return false;
    }

    // 2.
    for (const auto &ComputeMNode : this->ComputeMetaNodes) {
      if (ComputeMNode.PHIMetaNodes.size() > 1) {
        return false;
      }
    }

    // 3.
    this->constructComputePath();
    const ComputePath *CurrentNonEmptyComputePath = nullptr;
    for (const auto &Path : this->AllComputePaths) {
      if (Path.isEmpty()) {
        continue;
      }
      if (CurrentNonEmptyComputePath == nullptr) {
        CurrentNonEmptyComputePath = &Path;
      } else {
        // We have to make sure that these two compute path are the same.
        if (!CurrentNonEmptyComputePath->isIdenticalTo(&Path)) {
          return false;
        }
      }
    }

    // 4. Only add,sub are allowed so far. TODO.
    if (CurrentNonEmptyComputePath != nullptr) {
    }

    return true;
  }

 private:
  const llvm::Loop *InnerMostLoop;
  const llvm::PHINode *PHINode;

  std::unordered_set<const llvm::LoadInst *> LoadInputs;
  std::unordered_set<const llvm::Instruction *> CallInputs;
  std::unordered_set<const llvm::Value *> LoopInvarianteInputs;
  std::unordered_set<const llvm::PHINode *> LoopHeaderPHIInputs;

  std::list<PHIMetaNode> PHIMetaNodes;
  std::list<ComputeMetaNode> ComputeMetaNodes;

  struct ComputePath {
    std::vector<ComputeMetaNode *> ComputeMetaNodes;
    bool isEmpty() const {
      for (const auto &ComputeMNode : this->ComputeMetaNodes) {
        if (!ComputeMNode->isEmpty()) {
          return false;
        }
      }
      return true;
    }
    bool isIdenticalTo(const ComputePath *Other) const {
      if (this->ComputeMetaNodes.size() != Other->ComputeMetaNodes.size()) {
        return false;
      }
      for (size_t ComputeMetaNodeIdx = 0,
                  NumComputeMetaNodes = this->ComputeMetaNodes.size();
           ComputeMetaNodeIdx != NumComputeMetaNodes; ++ComputeMetaNodeIdx) {
        const auto &ThisComputeMNode =
            this->ComputeMetaNodes.at(ComputeMetaNodeIdx);
        const auto &OtherComputeMNode =
            this->ComputeMetaNodes.at(ComputeMetaNodeIdx);
        if (!ThisComputeMNode->isIdenticalTo(OtherComputeMNode)) {
          return false;
        }
      }
      return true;
    }
  };

  std::list<ComputePath> AllComputePaths;

  struct DFSNode {
    ComputeMetaNode *ComputeMNode;
    const llvm::Value *Value;
    int VisitTimes;
    DFSNode(ComputeMetaNode *_ComputeMNode, const llvm::Value *_Value)
        : ComputeMNode(_ComputeMNode), Value(_Value), VisitTimes(0) {}
  };

  using ConstructedPHIMetaNodeMapT =
      std::unordered_map<const llvm::PHINode *, PHIMetaNode *>;
  using ConstructedComputeMetaNodeMapT =
      std::unordered_map<const llvm::Value *, ComputeMetaNode *>;

  void handleFirstTimeComputeNode(
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
            this->handleFirstTimePHIMetaNode(DFSStack,
                                             &this->PHIMetaNodes.back(),
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

  void handleFirstTimePHIMetaNode(
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
        this->ComputeMetaNodes.emplace_back();
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

  void constructMetaGraph() {
    // Create the first PHIMetaNode.
    ConstructedPHIMetaNodeMapT ConstructedPHIMetaNodeMap;
    ConstructedComputeMetaNodeMapT ConstructedComputeMetaNodeMap;
    this->PHIMetaNodes.emplace_back(this->PHINode);
    ConstructedPHIMetaNodeMap.emplace(this->PHINode,
                                      &this->PHIMetaNodes.back());
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

  void constructComputePath() {
    // Start from the root.
    assert(!this->PHIMetaNodes.empty() &&
           "Failed to find the root PHIMetaNode.");
    auto &RootPHIMetaNode = this->PHIMetaNodes.front();
    assert(RootPHIMetaNode.PHINode == this->PHINode &&
           "Mismatched PHINode at root PHIMetaNode.");
    ComputePath CurrentPath;
    for (auto &ComputeMNode : RootPHIMetaNode.ComputeMetaNodes) {
      this->constructComputePathRecursive(ComputeMNode, CurrentPath);
    }
  }

  void constructComputePathRecursive(ComputeMetaNode *ComputeMNode,
                                     ComputePath &CurrentPath) {
    assert(ComputeMNode->PHIMetaNodes.size() <= 1 &&
           "Can not handle compute path for more than one PHIMetaNode child.");
    // Add myself to the path.
    CurrentPath.ComputeMetaNodes.push_back(ComputeMNode);

    if (ComputeMNode->PHIMetaNodes.empty()) {
      // I am the end.
      // This will copy the current path.
      this->AllComputePaths.push_back(CurrentPath);
    } else {
      // Recursively go to the children.
      auto &PHIMNode = *(ComputeMNode->PHIMetaNodes.begin());
      assert(!PHIMNode->ComputeMetaNodes.empty() &&
             "Every PHIMetaNode should have ComputeMetaNode child.");
      for (auto &ComputeMNodeChild : PHIMNode->ComputeMetaNodes) {
        this->constructComputePathRecursive(ComputeMNodeChild, CurrentPath);
      }
    }

    // Remove my self from the path.
    CurrentPath.ComputeMetaNodes.pop_back();
  }
};
}  // namespace

bool InductionVarStream::isStaticStream() const {
  return StaticIVStreamAnalyzer(this->InnerMostLoop, this->PHIInst)
      .isStaticStream();
}
