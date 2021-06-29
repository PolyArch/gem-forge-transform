#ifndef GEM_FORGE_STREAM_LOOP_ELIMINATOR_H
#define GEM_FORGE_STREAM_LOOP_ELIMINATOR_H

#include "StreamExecutionTransformer.h"

class StreamLoopEliminator {
public:
  StreamLoopEliminator(StreamExecutionTransformer *_Transformer);

  void eliminateLoops(StaticStreamRegionAnalyzer *Analyzer);

private:
  StreamExecutionTransformer *Transformer;

  void eliminateLoop(StaticStreamRegionAnalyzer *Analyzer,
                     const llvm::Loop *Loop);

  bool canLoopBeEliminated(StaticStreamRegionAnalyzer *Analyzer,
                           const llvm::Loop *Loop,
                           const llvm::Loop *ClonedLoop);
};

#endif