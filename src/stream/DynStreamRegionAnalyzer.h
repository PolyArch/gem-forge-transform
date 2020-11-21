#ifndef LLVM_TDG_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STREAM_REGION_ANALYZER_H

#include "DataGraph.h"
#include "LoopUtils.h"
#include "Utils.h"

#include "stream/DynIndVarStream.h"
#include "stream/DynMemStream.h"
#include "stream/StaticStreamRegionAnalyzer.h"
#include "stream/ae/FunctionalStreamEngine.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

class DynStreamRegionAnalyzer {
public:
  DynStreamRegionAnalyzer(uint64_t _RegionIdx, CachedLoopInfo *_CachedLI,
                          CachedPostDominanceFrontier *_CachedPDF,
                          CachedBBPredicateDataGraph *_CachedBBPredDG,
                          llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
                          const std::string &_RootPath);

  DynStreamRegionAnalyzer(const DynStreamRegionAnalyzer &Other) = delete;
  DynStreamRegionAnalyzer(DynStreamRegionAnalyzer &&Other) = delete;
  DynStreamRegionAnalyzer &
  operator=(const DynStreamRegionAnalyzer &Other) = delete;
  DynStreamRegionAnalyzer &operator=(DynStreamRegionAnalyzer &&Other) = delete;

  ~DynStreamRegionAnalyzer();

  void addMemAccess(DynamicInstruction *DynamicInst, DataGraph *DG);
  void addIVAccess(DynamicInstruction *DynamicInst);
  void endIter(const llvm::Loop *Loop);
  void endLoop(const llvm::Loop *Loop);
  void
  finalizePlan(StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy);
  void endTransform();
  void finalizeCoalesceInfo();
  void coalesceStreamsAtLoop(llvm::Loop *Loop);
  void dumpStats() const;

  /**
   * Basic query API.
   */
  uint64_t getNumDynamicMemAccesses() const {
    return this->numDynamicMemAccesses;
  }

  llvm::Loop *getTopLoop() { return this->TopLoop; }

  /**
   * Query for the analysis results.
   */

  using InstTransformPlanMapT =
      StaticStreamRegionAnalyzer::InstTransformPlanMapT;

  const InstTransformPlanMapT &getInstTransformPlanMap() const {
    return this->InstPlanMap;
  }

  const StreamConfigureLoopInfo &
  getConfigureLoopInfo(const llvm::Loop *ConfigureLoop);

  DynStream *getChosenStreamByInst(const llvm::Instruction *Inst);
  DynStream *getDynStreamByStaticStream(StaticStream *SS);

  const StreamTransformPlan &
  getTransformPlanByInst(const llvm::Instruction *Inst);

  FunctionalStreamEngine *getFuncSE();

  /**
   * Insert address computation function in the module.
   * Used by StreamExeuctionPass.
   */
  void insertAddrFuncInModule(std::unique_ptr<llvm::Module> &Module);

private:
  CachedLoopInfo *CachedLI;
  CachedPostDominanceFrontier *CachedPDF;
  CachedBBPredicateDataGraph *CachedBBPredDG;
  llvm::Loop *TopLoop;
  llvm::LoopInfo *LI;
  llvm::DataLayout *DataLayout;
  std::unique_ptr<StaticStreamRegionAnalyzer> StaticAnalyzer;

  /**
   * Remember the number of dynamic memory accesses happened in this region.
   */
  uint64_t numDynamicMemAccesses = 0;

  /**
   * We are switching to StaticStream and simplify DynStream.
   * This remembers the map from StaticStream to DynStream.
   */
  std::unordered_map<StaticStream *, DynStream *> StaticDynStreamMap;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop streams.
   */
  std::unordered_map<const llvm::Instruction *, std::list<DynStream *>>
      InstStreamMap;

  /**
   * Map from an instruction to its chosen stream.
   */
  std::unordered_map<const llvm::Instruction *, DynStream *>
      InstChosenStreamMap;

  /**
   * Map from a loop to all the streams with this loop as the inner most loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<DynStream *>>
      InnerMostLoopStreamMap;

  /**
   * Map from a loop to all the streams configured at the entry point to this
   * loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<DynStream *>>
      ConfigureLoopStreamMap;

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

  std::unique_ptr<llvm::Interpreter> AddrInterpreter;
  std::unique_ptr<FunctionalStreamEngine> FuncSE;

  void initializeStreams();

  DynStream *
  getStreamByInstAndConfigureLoop(const llvm::Instruction *Inst,
                                  const llvm::Loop *ConfigureLoop) const;
  void buildStreamAddrDepGraph();

  void markQualifiedStreams(
      StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy,
      StreamPassChooseStrategyE StreamPassChooseStrategy);
  void disqualifyStreams();

  void chooseStreamAtInnerMost();
  void chooseStreamAtDynamicOuterMost();
  void chooseStreamAtStaticOuterMost();

  void buildChosenStreamDependenceGraph();

  void buildAddressModule();

  void buildTransformPlan();

  void buildStreamConfigureLoopInfoMap(const llvm::Loop *ConfigureLoop);
  // Must be called after allocate the StreamConfigureLoopInfo.
  void allocateRegionStreamId(const llvm::Loop *ConfigureLoop);

  std::vector<DynStream *>
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
  std::string classifyStream(const DynMemStream &S) const;
};

#endif