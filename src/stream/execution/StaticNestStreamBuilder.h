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

  struct NestPredicationResult {
    bool IsPredicated = false;
    bool PredicateRet = false;
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> PredFuncInfo;
    std::vector<const llvm::Value *> PredInputValues;
  };

  bool checkNestPredication(StaticStreamRegionAnalyzer *Analyzer,
                            const llvm::Loop *OuterLoop,
                            const llvm::Loop *InnerLoop,
                            NestPredicationResult &ret);

  bool checkNestPredicationForOuterBB(StaticStreamRegionAnalyzer *Analyzer,
                                      const llvm::Loop *OuterLoop,
                                      const llvm::Loop *InnerLoop,
                                      const llvm::BasicBlock *OuterBB,
                                      NestPredicationResult &ret);
};

#endif