#ifndef LLVM_TDG_STREAM_USER_DEFINED_MEM_STREAM_H
#define LLVM_TDG_STREAM_USER_DEFINED_MEM_STREAM_H

#include "stream/StaticStream.h"

/**
 * A MemStream directly defined by the user/programmer.
 * We omit "Static" as this is always static.
 * So far we only support simple linear stream.
 */
class UserDefinedMemStream : public StaticStream {
public:
  UserDefinedMemStream(const llvm::Instruction *_Inst,
                       const llvm::Loop *_ConfigureLoop,
                       const llvm::Loop *_InnerMostLoop,
                       llvm::ScalarEvolution *_SE,
                       const llvm::PostDominatorTree *_PDT,
                       llvm::DataLayout *_DataLayout)
      : StaticStream(TypeT::MEM, _Inst, _ConfigureLoop, _InnerMostLoop, _SE,
                     _PDT, _DataLayout) {}

  /**
   * Of course we are always qualified.
   */
  bool checkIsQualifiedWithoutBackEdgeDep() const override { return true; }
  bool checkIsQualifiedWithBackEdgeDep() const override { return true; }
  void finalizePattern() override;

  /**
   * Check if this is a user-defined memory stream.
   * Basically this is a call to ssp_stream_load_xx.
   */
  static bool isUserDefinedMemStream(const llvm::Instruction *Inst);

private:
  /**
   * Initialize the DFSStack by pushing the first ComputeMetaGraph.
   */
  void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) override;

  /**
   * Of course I am always a candidate.
   */
  void analyzeIsCandidate() override { this->IsCandidate = true; }

  LLVM::TDG::StreamStepPattern computeStepPattern() const override;

  static const std::unordered_set<std::string> StreamLoadFuncNames;
};

#endif