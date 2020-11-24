#ifndef LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STATIC_STREAM_REGION_ANALYZER_H

#include "stream/StaticIndVarStream.h"
#include "stream/StaticMemStream.h"
#include "stream/UserDefinedMemStream.h"

#include "stream/StreamConfigureLoopInfo.h"
#include "stream/StreamTransformPlan.h"

class StaticStreamRegionAnalyzer {
public:
  using InstStaticStreamMapT =
      std::unordered_map<const llvm::Instruction *, std::list<StaticStream *>>;
  StaticStreamRegionAnalyzer(llvm::Loop *_TopLoop,
                             llvm::DataLayout *_DataLayout,
                             CachedLoopInfo *_CachedLI,
                             CachedPostDominanceFrontier *_CachedPDF,
                             CachedBBPredicateDataGraph *_CachedBBPredDG,
                             uint64_t _RegionIdx, const std::string &_RootPath);
  ~StaticStreamRegionAnalyzer();

  InstStaticStreamMapT &getInstStaticStreamMap() {
    return this->InstStaticStreamMap;
  }

  /**
   * This function finalizes the transformation plan.
   * 1. Choose streams based on the ChooseStrategy, and build the chosen stream
   * dependence graph.
   * 2. Build the transform plan.
   */
  void finalizePlan();

  void endTransform();

  void coalesceStreamsAtLoop(llvm::Loop *Loop);

  void insertPredicateFuncInModule(std::unique_ptr<llvm::Module> &Module);
  /**
   * Insert address computation function in the module.
   * Used by StreamExeuctionPass.
   */
  void insertAddrFuncInModule(std::unique_ptr<llvm::Module> &Module);

  /**
   * Query for the analysis results.
   */
  StaticStream *getChosenStreamByInst(const llvm::Instruction *Inst) {
    if (!this->TopLoop->contains(Inst)) {
      return nullptr;
    }
    auto Iter = this->InstChosenStreamMap.find(Inst);
    if (Iter == this->InstChosenStreamMap.end()) {
      return nullptr;
    } else {
      return Iter->second;
    }
  }

  using InstTransformPlanMapT =
      std::unordered_map<const llvm::Instruction *, StreamTransformPlan>;

  const InstTransformPlanMapT &getInstTransformPlanMap() const {
    return this->InstPlanMap;
  }
  const StreamTransformPlan &
  getTransformPlanByInst(const llvm::Instruction *Inst) {
    assert(this->TopLoop->contains(Inst) &&
           "Inst should be within the TopLoop.");
    return this->InstPlanMap.at(Inst);
  }

  const StreamConfigureLoopInfo &
  getConfigureLoopInfo(const llvm::Loop *ConfigureLoop);

  llvm::Loop *getTopLoop() { return this->TopLoop; }
  const std::string &getAnalyzePath() const { return this->AnalyzePath; }
  const std::string &getAnalyzeRelativePath() const {
    return this->AnalyzeRelativePath;
  }

protected:
  llvm::Loop *TopLoop;
  llvm::DataLayout *DataLayout;
  CachedLoopInfo *CachedLI;
  CachedBBPredicateDataGraph *CachedBBPredDG;
  llvm::LoopInfo *LI;
  llvm::ScalarEvolution *SE;
  const llvm::PostDominatorTree *PDT;

  uint64_t RegionIdx;
  std::string RootPath;
  std::string AnalyzeRelativePath;
  std::string AnalyzePath;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop.
   */
  InstStaticStreamMapT InstStaticStreamMap;

  /**
   * Map from an instruction to its chosen stream.
   */
  std::unordered_map<const llvm::Instruction *, StaticStream *>
      InstChosenStreamMap;

  /**
   * Map from a loop to all the streams with this loop as the inner most loop.
   */
  using LoopStreamMapT = std::unordered_map<const llvm::Loop *,
                                            std::unordered_set<StaticStream *>>;
  LoopStreamMapT InnerMostLoopStreamMap;

  /**
   * Map from a loop to all the streams configured at the entry point to this
   * loop.
   */
  LoopStreamMapT ConfigureLoopStreamMap;

  /**
   * Map from a configure loop to the aggregated structure
   * StreamConfigureLoopInfo.
   */
  std::unordered_map<const llvm::Loop *, StreamConfigureLoopInfo>
      ConfigureLoopInfoMap;

  /**
   * Map the instruction to the transform plan.
   */
  InstTransformPlanMapT InstPlanMap;

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
  void chooseStreams();
  void chooseStreamAtInnerMost();
  void chooseStreamAtStaticOuterMost();
  void buildChosenStreamDependenceGraph();
  void buildTransformPlan();

  /**
   * Finalize the StreamConfigureLoopInfoMap.
   */
  void buildStreamConfigureLoopInfoMap(const llvm::Loop *ConfigureLoop);
  // Must be called after allocate the StreamConfigureLoopInfo.
  void allocateRegionStreamId(const llvm::Loop *ConfigureLoop);
  std::vector<StaticStream *>
  sortChosenStreamsByConfigureLoop(const llvm::Loop *ConfigureLoop);

  /**
   * Finalize the StreamConfigureLoopInfo after the transformation.
   * Mainly compute the number of peer streams and peer coalesced streams.
   * Coalesce information is only available after transformation.
   */
  void finalizeStreamConfigureLoopInfo(const llvm::Loop *ConfigureLoop);

  void dumpTransformPlan();
  void dumpConfigurePlan();
  void dumpStreamInfos();
};

#endif