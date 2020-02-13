#ifndef LLVM_TDG_STREAM_STATIC_MEM_STREAM_H
#define LLVM_TDG_STREAM_STATIC_MEM_STREAM_H

#include "stream/StaticStream.h"

class StaticMemStream : public StaticStream {
public:
  StaticMemStream(const llvm::Instruction *_Inst,
                  const llvm::Loop *_ConfigureLoop,
                  const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE,
                  const llvm::PostDominatorTree *_PDT)
      : StaticStream(TypeT::MEM, _Inst, _ConfigureLoop, _InnerMostLoop, _SE,
                     _PDT) {}

  bool checkIsQualifiedWithoutBackEdgeDep() const override;
  bool checkIsQualifiedWithBackEdgeDep() const override;
  void finalizePattern() override;

private:
  /**
   * Initialize the DFSStack by pushing the first ComputeMetaGraph.
   */
  void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) override;

  void analyzeIsCandidate() override;

  /**
   * Helper function to validate SCEV can be represented by as a
   * stream datagraph.
   *
   * A SCEV is a valid StreamDG if it recursively:
   * 1. SCEVCommutativeExpr:
   *   a. One of the operand is LoopInvariant or within the InputSCEVs.
   *   b. The other operand is StreamDG.
   * 2. SCEVCastExpr: The operand is StreamDG.
   * 3. SCEV that within the InputSCEVs.
   * 4. AddRecSCEV.
   */
  bool validateSCEVAsStreamDG(
      const llvm::SCEV *SCEV,
      const std::unordered_set<const llvm::SCEV *> &InputSCEVs);

  LLVM::TDG::StreamStepPattern computeStepPattern() const override;
};

#endif