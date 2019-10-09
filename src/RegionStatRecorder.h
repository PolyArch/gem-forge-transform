#ifndef LLVM_TDG_REGION_STAT_RECORDER_H
#define LLVM_TDG_REGION_STAT_RECORDER_H

#include "LoopUtils.h"

#include <unordered_map>
#include <iomanip>

/**
 * As now we merge ADFA into Replay, we use this to record some basic
 * statistic for regions.
 */
class RegionStatRecorder {
public:
  struct DataFlowStats {
    std::string RegionId;
    uint64_t StaticInst;
    uint64_t DynamicInst;
    uint64_t DataFlowDynamicInst;
    uint64_t Config;
    uint64_t DataFlow;

    DataFlowStats(const std::string &_RegionId, uint64_t _StaticInst)
        : RegionId(_RegionId), StaticInst(_StaticInst), DynamicInst(0),
          DataFlowDynamicInst(0), Config(0), DataFlow(0) {}

    static void dumpStatsTitle(std::ostream &O) {
      O << std::setw(50) << "RegionId" << std::setw(20) << "StaticInsts"
        << std::setw(20) << "DynamicInst" << std::setw(20)
        << "DataFlowDynamicInst" << std::setw(20) << "Config" << std::setw(20)
        << "DataFlow" << '\n';
    }

    void dumpStats(std::ostream &O) const {
      O << std::setw(50) << this->RegionId << std::setw(20) << this->StaticInst
        << std::setw(20) << this->DynamicInst << std::setw(20)
        << this->DataFlowDynamicInst << std::setw(20) << this->Config
        << std::setw(20) << this->DataFlow << '\n';
    }
  };

  void addInst(const llvm::Loop *Region, const llvm::Instruction *StaticInst);
  void dumpStats(std::ostream &O);

private:
  std::unordered_map<const llvm::Loop *, DataFlowStats> Stats;
  const llvm::Loop *CurrentRegion = nullptr;
  bool InRegion = false;
};

#endif