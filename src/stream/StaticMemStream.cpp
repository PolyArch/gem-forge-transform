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

void StaticMemStream::analyze() {
  /**
   * Start to check constraints:
   * 1. No PHIMetaNode.
   */
  if (!this->PHIMetaNodes.empty()) {
    this->IsStream = false;
    return;
  }
  /**
   * At most one LoopHeaderPHIInput.
   */
  if (this->LoopHeaderPHIInputs.size() > 1) {
    this->IsStream = false;
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
    this->IsStream = true;
    this->ValPattern = LLVM::TDG::StreamValuePattern::CONSTANT;
    return;
  }
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
