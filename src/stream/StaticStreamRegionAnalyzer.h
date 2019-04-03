#ifndef LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H

#include "stream/StaticStream.h"

class StaticStreamRegionAnalyzer {
public:
  using InstStaticStreamMapT =
      std::unordered_map<const llvm::Instruction *, std::list<StaticStream *>>;
  StaticStreamRegionAnalyzer(llvm::Loop *_TopLoop,
                             llvm::DataLayout *_DataLayout,
                             CachedLoopInfo *_CachedLI);
  ~StaticStreamRegionAnalyzer();

  InstStaticStreamMapT &getInstStaticStreamMap() {
    return this->InstStaticStreamMap;
  }

private:
  llvm::Loop *TopLoop;
  llvm::DataLayout *DataLayout;
  CachedLoopInfo *CachedLI;
  llvm::LoopInfo *LI;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop.
   */
  InstStaticStreamMapT InstStaticStreamMap;

  void initializeStreams();
  void initializeStreamForAllLoops(const llvm::Instruction *StreamInst);
};

#endif