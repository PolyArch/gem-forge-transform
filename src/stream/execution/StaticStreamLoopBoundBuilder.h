#ifndef GEM_FORGE_STATIC_LOOP_BOUND_STREAM_BUILDER_H
#define GEM_FORGE_STATIC_LOOP_BOUND_STREAM_BUILDER_H

#include "StreamExecutionTransformer.h"

class StaticStreamLoopBoundBuilder {
public:
  StaticStreamLoopBoundBuilder(StreamExecutionTransformer *_Transformer);

  /**
   * Helper functions to build LoopBoundDGs.
   */
  using InputValueVec = StreamExecutionTransformer::InputValueVec;
  void buildStreamLoopBoundForLoop(StaticStreamRegionAnalyzer *Analyzer,
                                   const llvm::Loop *Loop,
                                   InputValueVec &ClonedInputValues);

private:
  StreamExecutionTransformer *Transformer;

  bool isStreamLoopBound(StaticStreamRegionAnalyzer *Analyzer,
                         const llvm::Loop *Loop,
                         BBBranchDataGraph *LatchBBBranchDG);
};

#endif