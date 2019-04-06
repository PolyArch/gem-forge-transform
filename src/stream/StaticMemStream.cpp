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
  if (this->ConfigureLoop != this->InnerMostLoop) {
    this->IsCandidate = false;
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
   * At most one LoopHeaderPHIInput.
   */
  if (this->LoopHeaderPHIInputs.size() > 1) {
    this->IsCandidate = false;
    return;
  }
  assert(this->ComputeMetaNodes.size() == 1 &&
         "StaticMemStream should always have one ComputeMetaNode.");
  const auto &ComputeMNode = this->ComputeMetaNodes.front();
  auto SCEV = ComputeMNode.SCEV;
  assert(SCEV != nullptr && "StaticMemStream should always has SCEV.");
  if (this->SE->isLoopInvariant(SCEV, this->InnerMostLoop)) {
    // Constant stream.
    assert(this->LoopHeaderPHIInputs.empty() &&
           "Constant StaticMemStream should have no LoopHeaderPHIInput.");
    this->IsCandidate = true;
    this->ValPattern = LLVM::TDG::StreamValuePattern::CONSTANT;
    return;
  }
  /**
   * Otherwise, we allow the following SCEV expr:
   * 1. SCEV on LoopHeaderPHINode.
   * 2. AddRecExpr.
   * 3. AddExpr with one loop invariant and the other LoopHeaderPHINode.
   */
  if (this->LoopHeaderPHIInputs.size() == 1) {
    auto LoopHeaderPHINode =
        const_cast<llvm::PHINode *>(*(this->LoopHeaderPHIInputs.begin()));
    if (!this->SE->isSCEVable(LoopHeaderPHINode->getType())) {
      this->IsCandidate = false;
      return;
    }
    auto LoopHeaderPHINodeSCEV = this->SE->getSCEV(LoopHeaderPHINode);
    if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
      // 2. AddRecExpr.
      if (AddRecSCEV->isAffine()) {
        this->IsCandidate = true;
        this->ValPattern = LLVM::TDG::StreamValuePattern::LINEAR;
        return;
      }
    } else {
      std::unordered_set<const llvm::SCEV *> InputSCEVs;
      InputSCEVs.insert(LoopHeaderPHINodeSCEV);
      // Also insert the loads' SCEV.
      for (auto &LoadInst : this->LoadInputs) {
        if (this->SE->isSCEVable(LoadInst->getType())) {
          auto LoadSCEV =
              this->SE->getSCEV(const_cast<llvm::LoadInst *>(LoadInst));
          InputSCEVs.insert(LoadSCEV);
        }
      }
      if (this->validateSCEVAsStreamDG(SCEV, InputSCEVs)) {
        this->IsCandidate = true;
        return;
      }
    }
  } else {
    std::unordered_set<const llvm::SCEV *> InputSCEVs;
    // Also insert the loads' SCEV.
    for (auto &LoadInst : this->LoadInputs) {
      if (this->SE->isSCEVable(LoadInst->getType())) {
        auto LoadSCEV =
            this->SE->getSCEV(const_cast<llvm::LoadInst *>(LoadInst));
        InputSCEVs.insert(LoadSCEV);
      }
    }
    if (this->validateSCEVAsStreamDG(SCEV, InputSCEVs)) {
      this->IsCandidate = true;
      return;
    }
  }

  this->IsCandidate = false;
  return;
}

void StaticMemStream::buildDependenceGraph(GetStreamFuncT GetStream) {
  for (const auto &LoopHeaderPHIInputs : this->LoopHeaderPHIInputs) {
    auto BaseIVStream = GetStream(LoopHeaderPHIInputs, this->ConfigureLoop);
    assert(BaseIVStream != nullptr && "Missing base IVStream.");
    this->addBaseStream(BaseIVStream);
  }
  for (const auto &LoadInput : this->LoadInputs) {
    auto BaseMStream = GetStream(LoadInput, this->ConfigureLoop);
    assert(BaseMStream != nullptr && "Missing base MemStream.");
    this->addBaseStream(BaseMStream);
  }
}

bool StaticMemStream::validateSCEVAsStreamDG(
    const llvm::SCEV *SCEV,
    const std::unordered_set<const llvm::SCEV *> &InputSCEVs) {
  assert(!this->SE->isLoopInvariant(SCEV, this->InnerMostLoop) &&
         "Only validate LoopVariant SCEV.");
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
        if (this->SE->isLoopInvariant(OpSCEV, this->InnerMostLoop)) {
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
      return false;
    }
  }
  return true;
}

bool StaticMemStream::checkIsQualifiedWithBackEdgeDep() const {
  // MemStream should have no back edge dependence.
  assert(this->BackMemBaseStreams.empty() &&
         "Memory stream should not have back edge base stream.");
  return this->checkIsQualifiedWithoutBackEdgeDep();
}

void StaticMemStream::finalizePattern() {
  // Copy the step pattern from the step root.
  if (this->BaseStepRootStreams.empty()) {
    this->StpPattern = LLVM::TDG::StreamStepPattern::NEVER;
  } else {
    assert(this->BaseStepRootStreams.size() == 1 &&
           "More than one step root stream to finalize pattern.");
    auto StepRootStream = *(this->BaseStepRootStreams.begin());
    this->StpPattern = StepRootStream->StpPattern;
  }
  // Compute the value pattern.
  this->ValPattern = LLVM::TDG::StreamValuePattern::CONSTANT;
  for (auto &BaseStream : this->BaseStreams) {
    if (BaseStream->Type == MEM) {
      // This makes me indirect stream.
      this->ValPattern = LLVM::TDG::StreamValuePattern::INDIRECT;
      break;
    } else {
      // This is likely our step root, copy its value pattern.
      this->ValPattern = BaseStream->ValPattern;
    }
  }
}