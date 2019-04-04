#include "stream/StaticStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/raw_ostream.h"

void StaticStream::setStaticStreamInfo(LLVM::TDG::StaticStreamInfo &SSI) const {
  SSI.set_is_candidate(this->IsCandidate);
  SSI.set_is_qualified(this->IsQualified);
  SSI.set_stp_pattern(this->StpPattern);
  SSI.set_val_pattern(this->ValPattern);
}

void StaticStream::handleFirstTimeComputeNode(
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
        // We have to pop this before handleFirstTimePHIMetaNode emplace next
        // compute nodes.
        DFSStack.pop_back();
        if (ConstructedPHIMetaNodeMap.count(PHINode) == 0) {
          // First time encounter the phi node.
          this->PHIMetaNodes.emplace_back(PHINode);
          ConstructedPHIMetaNodeMap.emplace(PHINode,
                                            &this->PHIMetaNodes.back());
          // This may modify the DFSStack.
          this->handleFirstTimePHIMetaNode(DFSStack, &this->PHIMetaNodes.back(),
                                           ConstructedComputeMetaNodeMap);
        }
        auto PHIMNode = ConstructedPHIMetaNodeMap.at(PHINode);
        ComputeMNode->PHIMetaNodes.insert(PHIMNode);
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

void StaticStream::handleFirstTimePHIMetaNode(
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

void StaticStream::constructMetaGraph() {
  // Create the first PHIMetaNode.
  ConstructedPHIMetaNodeMapT ConstructedPHIMetaNodeMap;
  ConstructedComputeMetaNodeMapT ConstructedComputeMetaNodeMap;
  std::list<DFSNode> DFSStack;
  this->initializeMetaGraphConstruction(DFSStack, ConstructedPHIMetaNodeMap,
                                        ConstructedComputeMetaNodeMap);
  while (!DFSStack.empty()) {
    auto &DNode = DFSStack.back();
    if (DNode.VisitTimes == 0) {
      // First time.
      DNode.VisitTimes++;
      // Notice that handleFirstTimeComputeNode may pop DFSStack!.
      this->handleFirstTimeComputeNode(DFSStack, DNode,
                                       ConstructedPHIMetaNodeMap,
                                       ConstructedComputeMetaNodeMap);
    } else {
      // Second time. This must be a real compute inst.
      bool Inserted = false;
      auto Inst = llvm::dyn_cast<llvm::Instruction>(DNode.Value);
      assert(Inst && "Only compute inst will be handled twice.");
      assert(!llvm::isa<llvm::PHINode>(Inst) &&
             "No PHINode can be handled twice.");
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

/**
 * This must happen after all the calls to addBaseStream.
 */
void StaticStream::computeBaseStepRootStreams() {
  for (auto &BaseStepStream : this->BaseStepStreams) {
    if (BaseStepStream->Type == StaticStream::TypeT::IV) {
      // Induction variable is always a root stream.
      this->BaseStepRootStreams.insert(BaseStepStream);
      // No need to go deeper for IVStream.
      continue;
    }
    if (BaseStepStream->BaseStepRootStreams.empty()) {
      // If this is empty, recompute (even if recomputed).
      BaseStepStream->computeBaseStepRootStreams();
    }
    for (auto &BaseStepRootStream : BaseStepStream->BaseStepRootStreams) {
      this->BaseStepRootStreams.insert(BaseStepRootStream);
    }
  }
}
