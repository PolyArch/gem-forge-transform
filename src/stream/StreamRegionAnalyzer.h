#ifndef LLVM_TDG_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STREAM_REGION_ANALYZER_H

#include "DataGraph.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"
#include "stream/MemStream.h"
#include "stream/ae/FunctionalStreamEngine.h"

class StreamRegionAnalyzer {
public:
  StreamRegionAnalyzer(uint64_t _RegionIdx, llvm::Loop *_TopLoop,
                       llvm::LoopInfo *_LI, const std::string &_RootPath);

  StreamRegionAnalyzer(const StreamRegionAnalyzer &Other) = delete;
  StreamRegionAnalyzer(StreamRegionAnalyzer &&Other) = delete;
  StreamRegionAnalyzer &operator=(const StreamRegionAnalyzer &Other) = delete;
  StreamRegionAnalyzer &operator=(StreamRegionAnalyzer &&Other) = delete;

  ~StreamRegionAnalyzer();

  void addMemAccess(DynamicInstruction *DynamicInst, DataGraph *DG);
  void addIVAccess(DynamicInstruction *DynamicInst);
  void endIter(const llvm::Loop *Loop);
  void endLoop(const llvm::Loop *Loop);
  void endRegion();

private:
  uint64_t RegionIdx;
  llvm::Loop *TopLoop;
  llvm::LoopInfo *LI;
  std::string RootPath;
  std::string AnalyzePath;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop streams.
   */
  std::unordered_map<const llvm::Instruction *, std::list<Stream *>>
      InstStreamMap;

  /**
   * Map from a loop to all the streams configured at the entry point to this
   * loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<Stream *>>
      ConfiguredLoopStreamMap;

  /**
   * Map from a loop to all the streams with this loop as the inner most loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<Stream *>>
      InnerMostLoopStreamMap;

  void initializeStreams();
  void initializeStreamForAllLoops(const llvm::Instruction *StreamInst);
};

#endif