#ifndef LLVM_TDG_DYN_STREAM_REGION_ANALYZER_H
#define LLVM_TDG_DYN_STREAM_REGION_ANALYZER_H

#include "DataGraph.h"

#include "stream/DynIndVarStream.h"
#include "stream/DynMemStream.h"
#include "stream/StaticStreamRegionAnalyzer.h"
#include "stream/ae/FunctionalStreamEngine.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

/**
 * This is derived from StaticStreamRegionAnalyzer, but with dynamic
 * (trace) information to help analyze stream patterns.
 */

class DynStreamRegionAnalyzer : public StaticStreamRegionAnalyzer {
public:
  DynStreamRegionAnalyzer(llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
                          CachedLoopInfo *_CachedLI,
                          CachedPostDominanceFrontier *_CachedPDF,
                          CachedBBBranchDataGraph *_CachedBBBranchDG,
                          uint64_t _RegionIdx, const std::string &_RootPath);

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
  void endTransform() override;
  void finalizeCoalesceInfo();
  void dumpStats() const;

  /**
   * Basic query API.
   */
  uint64_t getNumDynamicMemAccesses() const {
    return this->numDynamicMemAccesses;
  }

  DynStream *getDynStreamByStaticStream(StaticStream *SS) const;

  FunctionalStreamEngine *getFuncSE();

private:
  /**
   * Remember the number of dynamic memory accesses happened in this region.
   */
  uint64_t numDynamicMemAccesses = 0;

  /**
   * This remembers the map from StaticStream to DynStream.
   */
  std::unordered_map<StaticStream *, DynStream *> StaticDynStreamMap;

  std::unique_ptr<llvm::Interpreter> AddrInterpreter;
  std::unique_ptr<FunctionalStreamEngine> FuncSE;

  void initializeStreams();

  DynStream *
  getDynStreamByInstAndConfigureLoop(const llvm::Instruction *Inst,
                                     const llvm::Loop *ConfigureLoop) const;

  void markQualifiedStreams(
      StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy,
      StreamPassChooseStrategyE StreamPassChooseStrategy);
  void disqualifyStreams();

  void chooseStreamAtInnerMost();
  void chooseStreamAtDynamicOuterMost();
  void chooseStreamAtStaticOuterMost();

  void buildChosenStreamDependenceGraph();

  std::string classifyStream(const DynMemStream &S) const;
};

#endif