#ifndef LLVM_TDG_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_STREAM_REGION_ANALYZER_H

#include "DataGraph.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"
#include "stream/MemStream.h"
#include "stream/StreamTransformPlan.h"
#include "stream/ae/FunctionalStreamEngine.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

enum StreamPassChooseStrategyE { OUTER_MOST, INNER_MOST };

class StreamRegionAnalyzer {
public:
  StreamRegionAnalyzer(uint64_t _RegionIdx, llvm::Loop *_TopLoop,
                       llvm::LoopInfo *_LI, llvm::DataLayout *_DataLayout,
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
  void endRegion(StreamPassChooseStrategyE StreamPassChooseStrategy);
  void endTransform();
  void dumpStats() const;

  /**
   * Query for the analysis results.
   */

  using InstTransformPlanMapT =
      std::unordered_map<const llvm::Instruction *, StreamTransformPlan>;

  const InstTransformPlanMapT &getInstTransformPlanMap() const {
    return this->InstPlanMap;
  }

  std::list<Stream *>
  getSortedChosenStreamsByConfiguredLoop(const llvm::Loop *ConfiguredLoop);

  Stream *getChosenStreamByInst(const llvm::Instruction *Inst);

  const StreamTransformPlan &
  getTransformPlanByInst(const llvm::Instruction *Inst);

  FunctionalStreamEngine *getFuncSE();

private:
  uint64_t RegionIdx;
  llvm::Loop *TopLoop;
  llvm::LoopInfo *LI;
  llvm::DataLayout *DataLayout;
  std::string RootPath;
  std::string AnalyzePath;

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
      ConfiguredLoopStreamMap;

  /**
   * Map the instruction to the transform plan.
   */
  InstTransformPlanMapT InstPlanMap;

  std::unique_ptr<llvm::Interpreter> AddrInterpreter;
  std::unique_ptr<FunctionalStreamEngine> FuncSE;

  void initializeStreams();
  void initializeStreamForAllLoops(const llvm::Instruction *StreamInst);

  Stream *
  getStreamByInstAndConfiguredLoop(const llvm::Instruction *Inst,
                                   const llvm::Loop *ConfiguredLoop) const;
  void buildStreamDependenceGraph();

  void markQualifiedStreams();
  void disqualifyStreams();

  void chooseStreamAtInnerMost();
  void chooseStreamAtOuterMost();

  void buildChosenStreamDependenceGraph();

  void buildAddressModule();

  void buildTransformPlan();

  void dumpTransformPlan();
  void dumpStreamInfos();
  std::string classifyStream(const MemStream &S) const;
};

#endif