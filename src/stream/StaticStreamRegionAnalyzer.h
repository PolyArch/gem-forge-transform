#ifndef LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H

#include "stream/StaticIndVarStream.h"
#include "stream/StaticMemStream.h"
#include "stream/UserDefinedMemStream.h"

class StaticStreamRegionAnalyzer {
public:
  using InstStaticStreamMapT =
      std::unordered_map<const llvm::Instruction *, std::list<StaticStream *>>;
  StaticStreamRegionAnalyzer(llvm::Loop *_TopLoop,
                             llvm::DataLayout *_DataLayout,
                             CachedLoopInfo *_CachedLI,
                             CachedPostDominanceFrontier *_CachedPDF,
                             CachedBBPredicateDataGraph *_CachedBBPredDG);
  ~StaticStreamRegionAnalyzer();

  InstStaticStreamMapT &getInstStaticStreamMap() {
    return this->InstStaticStreamMap;
  }

  void insertPredicateFuncInModule(std::unique_ptr<llvm::Module> &Module);

private:
  llvm::Loop *TopLoop;
  llvm::DataLayout *DataLayout;
  CachedLoopInfo *CachedLI;
  CachedBBPredicateDataGraph *CachedBBPredDG;
  llvm::LoopInfo *LI;
  llvm::ScalarEvolution *SE;
  const llvm::PostDominatorTree *PDT;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop.
   */
  InstStaticStreamMapT InstStaticStreamMap;

  StaticStream *
  getStreamByInstAndConfigureLoop(const llvm::Instruction *Inst,
                                  const llvm::Loop *ConfigureLoop) const;

  void initializeStreams();
  void initializeStreamForAllLoops(llvm::Instruction *StreamInst);
  void markUpdateRelationship();
  void markUpdateRelationshipForLoadStream(StaticStream *LoadSS);
  void buildStreamAddrDepGraph();
  void markAliasRelationship();
  void markAliasRelationshipForLoopBB(const llvm::Loop *Loop);
  void buildStreamValueDepGraph();
  void buildValueDepForStoreOrAtomic(StaticStream *StoreS);
  void markPredicateRelationship();
  void markPredicateRelationshipForLoopBB(const llvm::Loop *Loop,
                                          const llvm::BasicBlock *BB);
  void analyzeIsCandidate();
  void markQualifiedStreams();
  void enforceBackEdgeDependence();
};

#endif