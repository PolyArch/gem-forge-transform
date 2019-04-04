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
    this->analyze();
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

  void analyze();
};

#endif