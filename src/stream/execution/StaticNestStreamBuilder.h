#ifndef GEM_FORGE_STATIC_NEST_STREAM_BUILDER_H
#define GEM_FORGE_STATIC_NEST_STREAM_BUILDER_H

#include "StreamExecutionTransformer.h"

class StaticNestStreamBuilder {
public:
  StaticNestStreamBuilder(StreamExecutionTransformer *_Transformer);
  /**
   * Helper functions to build nest streams.
   */
  void buildNestStreams(StaticStreamRegionAnalyzer *Analyzer);

private:
  StreamExecutionTransformer *Transformer;

  void buildNestStreamsForLoop(StaticStreamRegionAnalyzer *Analyzer,
                               const llvm::Loop *Loop);
  bool canStreamsBeNested(StaticStreamRegionAnalyzer *Analyzer,
                          const llvm::Loop *OuterLoop,
                          const llvm::Loop *InnerLoop);
};

#endif