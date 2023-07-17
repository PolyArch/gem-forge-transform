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
  UserDefinedMemStream(StaticStreamRegionAnalyzer *_Analyzer,
                       llvm::Instruction *_Inst,
                       const llvm::Loop *_ConfigureLoop,
                       const llvm::Loop *_InnerMostLoop,
                       llvm::ScalarEvolution *_SE,
                       const llvm::PostDominatorTree *_PDT,
                       llvm::DataLayout *_DataLayout);

  /**
   * Of course we are always qualified.
   */
  bool checkIsQualifiedWithoutBackEdgeDep() const override { return true; }
  bool checkIsQualifiedWithBackEdgeDep() const override { return true; }
  void finalizePattern() override;
  const InstSet &getComputeInsts() const { return this->ComputeInsts; }

  /**
   * Check if this is a user-defined memory stream.
   * Basically this is a call to ssp_stream_load_xx.
   */
  static bool isUserDefinedMemStream(const llvm::Instruction *Inst);

  const llvm::CallInst *getConfigInst() const { return this->ConfigInst; }
  const llvm::CallInst *getEndInst() const { return this->EndInst; }

private:
  /**
   * So far we allow one user defined config/end instructions, and multiple
   * step instructions.
   */
  const llvm::CallInst *UserInst;
  const llvm::CallInst *ConfigInst;
  const llvm::CallInst *EndInst;

  /**
   * UserDefinedStream actually has no ComputeInsts. This is to implement
   * the getComputeInsts() interface.
   */
  InstSet ComputeInsts;

  /**
   * Initialize the DFSStack by pushing the first ComputeMetaGraph.
   */
  void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) override;

  /**
   * Search for config/end/step instructions for this stream.
   */
  void searchUserDefinedControlInstructions();

  /**
   * Check if I am a candidate.
   */
  void analyzeIsCandidate() override;

  LLVM::TDG::StreamStepPattern computeStepPattern() const override;

  static const std::unordered_set<std::string> StreamLoadFuncNames;
};

#endif