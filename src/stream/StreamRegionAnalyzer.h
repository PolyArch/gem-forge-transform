#ifndef LLVM_TDG_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STREAM_REGION_ANALYZER_H

#include "DataGraph.h"
#include "LoopUtils.h"
#include "Utils.h"

#include "stream/IndVarStream.h"
#include "stream/MemStream.h"
#include "stream/StaticStreamRegionAnalyzer.h"
#include "stream/StreamTransformPlan.h"
#include "stream/ae/FunctionalStreamEngine.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

/**
 * Aggreate all the information for a stream configure loop.
 */
class StreamConfigureLoopInfo {
public:
  StreamConfigureLoopInfo(const std::string &_Folder,
                          const std::string &_RelativeFolder,
                          const llvm::Loop *_Loop,
                          std::list<Stream *> _SortedStreams);

  const std::string &getRelativePath() const { return this->RelativePath; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const std::list<Stream *> getSortedStreams() const {
    return this->SortedStreams;
  }

  /**
   * TotalConfiguredStreams: Sum of all configured streams in this loop and
   * parent loops.
   * TotalSubLoopStreams: Maximum number of possible streams configured in
   * this loop and sub-loops.
   * TotalPeerStreams: Maximum number of possible streams be alive the
   * same time with streams configured in this loop.
   *
   * Formula:
   * * TotalConfiguredStreams = Sum(ConfiguredStreams, ThisAndParentLoop)
   * * TotalSubLoopStreams = \
   * *   (Max(TotalSubLoopStreams, SubLoop) or 0) + ConfiguredStreams
   * * TotalAliveStreams = TotalConfiguredStreams + \
   * *   TotalSubLoopStreams - COnfiguredStreams
   */
  int TotalConfiguredStreams;
  int TotalConfiguredCoalescedStreams;
  int TotalSubLoopStreams;
  int TotalSubLoopCoalescedStreams;
  int TotalAliveStreams;
  int TotalAliveCoalescedStreams;
  std::list<Stream *> SortedCoalescedStreams;

  void dump(llvm::DataLayout *DataLayout) const;

private:
  const std::string Path;
  const std::string RelativePath;
  const std::string JsonPath;
  const llvm::Loop *Loop;
  std::list<Stream *> SortedStreams;
};

class StreamRegionAnalyzer {
public:
  StreamRegionAnalyzer(uint64_t _RegionIdx, CachedLoopInfo *_CachedLI,
                       CachedPostDominanceFrontier *_CachedPDF,
                       llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
                       const std::string &_RootPath);

  StreamRegionAnalyzer(const StreamRegionAnalyzer &Other) = delete;
  StreamRegionAnalyzer(StreamRegionAnalyzer &&Other) = delete;
  StreamRegionAnalyzer &operator=(const StreamRegionAnalyzer &Other) = delete;
  StreamRegionAnalyzer &operator=(StreamRegionAnalyzer &&Other) = delete;

  ~StreamRegionAnalyzer();

  void addMemAccess(DynamicInstruction *DynamicInst, DataGraph *DG);
  void addIVAccess(DynamicInstruction *DynamicInst);
  void endIter(const llvm::Loop *Loop);
  void endLoop(const llvm::Loop *Loop);
  void endRegion(StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy,
                 StreamPassChooseStrategyE StreamPassChooseStrategy);
  void endTransform();
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
      std::unordered_map<const llvm::Instruction *, StreamTransformPlan>;

  const InstTransformPlanMapT &getInstTransformPlanMap() const {
    return this->InstPlanMap;
  }

  const StreamConfigureLoopInfo &
  getConfigureLoopInfo(const llvm::Loop *ConfigureLoop);

  Stream *getChosenStreamByInst(const llvm::Instruction *Inst);

  const StreamTransformPlan &
  getTransformPlanByInst(const llvm::Instruction *Inst);

  FunctionalStreamEngine *getFuncSE();

  /**
   * Insert address computation function in the module.
   * Used by StreamExeuctionPass.
   */
  void insertAddrFuncInModule(std::unique_ptr<llvm::Module> &Module);

private:
  uint64_t RegionIdx;
  CachedLoopInfo *CachedLI;
  CachedPostDominanceFrontier *CachedPDF;
  llvm::Loop *TopLoop;
  llvm::LoopInfo *LI;
  llvm::DataLayout *DataLayout;
  std::string RootPath;
  std::string AnalyzeRelativePath;
  std::string AnalyzePath;
  std::unique_ptr<StaticStreamRegionAnalyzer> StaticAnalyzer;

  /**
   * Remember the number of dynamic memory accesses happened in this region.
   */
  uint64_t numDynamicMemAccesses = 0;

  /**
   * Key data structure, map from instruction to the list of streams.
   * Starting from the inner-most loop streams.
   */
  std::unordered_map<const llvm::Instruction *, std::list<Stream *>>
      InstStreamMap;

  /**
   * Map from an instruction to its chosen stream.
   */
  std::unordered_map<const llvm::Instruction *, Stream *> InstChosenStreamMap;

  /**
   * Map from a loop to all the streams with this loop as the inner most loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<Stream *>>
      InnerMostLoopStreamMap;

  /**
   * Map from a loop to all the streams configured at the entry point to this
   * loop.
   */
  std::unordered_map<const llvm::Loop *, std::unordered_set<Stream *>>
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

  Stream *
  getStreamByInstAndConfigureLoop(const llvm::Instruction *Inst,
                                  const llvm::Loop *ConfigureLoop) const;
  void buildStreamDependenceGraph();

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

  std::list<Stream *>
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
  std::string classifyStream(const MemStream &S) const;
};

#endif