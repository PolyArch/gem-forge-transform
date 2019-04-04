#ifndef LLVM_TDG_STREAM_STATIC_MEM_STREAM_H
#define LLVM_TDG_STREAM_STATIC_MEM_STREAM_H

#include "stream/StaticStream.h"

class StaticMemStream : public StaticStream {
public:
  StaticMemStream(const llvm::Instruction *_Inst,
                  const llvm::Loop *_ConfigureLoop,
                  const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE)
      : StaticStream(TypeT::MEM, _Inst, _ConfigureLoop, _InnerMostLoop, _SE) {
    this->constructMetaGraph();
    this->analyzeIsCandidate();
  }

  void buildDependenceGraph(GetStreamFuncT GetStream) override;

private:
  /**
   * Initialize the DFSStack by pushing the first ComputeMetaGraph.
   */
  void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) override;

  void analyzeIsCandidate();

  /**
   * Helper function to validate SCEV can be represented by as a stream
   * datagraph. A SCEV is a valid StreamDG if it recursively:
   * 1. SCEVAddExpr:
   *   a. One of the operand is LoopInvariant.
   *   b. The other operand is StreamDG.
   * 2. SCEVMulExpr:
   *   a. One of the operand is LoopInvariant.
   *   b. The other operand is StreamDG.
   * 3. SCEVCastExpr:
   *   a. The operand is StreamDG.
   * 4. SCEV that equals the PHINodeInput.
   */
  bool validateSCEVAsStreamDG(const llvm::SCEV *SCEV,
                              const llvm::SCEV *PHINodeInputSCEV);
};

#endif