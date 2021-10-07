#include "RegionStatRecorder.h"

#include <map>

void RegionStatRecorder::addInst(const llvm::Loop *Region,
                                 const llvm::Instruction *StaticInst) {
  if (this->CurrentRegion && !this->CurrentRegion->contains(Region)) {
    this->InRegion = false;
  }
  if (!Region) {
    return;
  }
  if (!LoopUtils::isLoopContinuous(Region)) {
    return;
  }
  // Allocate and update the stats.
  {
    auto StatLoop = Region;
    while (StatLoop != nullptr) {
      if (!LoopUtils::isLoopContinuous(StatLoop)) {
        break;
      }
      if (this->Stats.find(StatLoop) == this->Stats.end()) {
        // Set the static instruction's count to 0 for now.
        this->Stats.emplace(
            StatLoop,
            DataFlowStats(LoopUtils::getLoopId(StatLoop),
                          LoopUtils::getNumStaticInstInLoop(StatLoop)));
        StatLoop = StatLoop->getParentLoop();
      } else {
        break;
      }
    }
  }

  // Update the configured loop.
  auto IsAtHeaderOfRegion = LoopUtils::isStaticInstLoopHead(Region, StaticInst);
  if (IsAtHeaderOfRegion) {
    if (!this->InRegion) {
      this->Stats.at(Region).DataFlow++;
    }
    if (!this->CurrentRegion || !this->CurrentRegion->contains(Region)) {
      this->CurrentRegion = Region;
      this->InRegion = true;
      this->Stats.at(Region).Config++;
    }
  }

  {
    // Update the stats.
    auto StatLoop = Region;
    while (StatLoop != nullptr) {
      if (!LoopUtils::isLoopContinuous(StatLoop)) {
        break;
      }
      auto &Stat = this->Stats.at(StatLoop);
      Stat.DynamicInst++;
      if (this->CurrentRegion && this->CurrentRegion->contains(StatLoop)) {
        Stat.DataFlowDynamicInst++;
      }
      StatLoop = StatLoop->getParentLoop();
    }
  }
}

void RegionStatRecorder::dumpStats(std::ostream &O) {
  DataFlowStats::dumpStatsTitle(O);
  // Sort the stats with according to their dynamic insts.
  std::multimap<uint64_t, const DataFlowStats *> SortedStats;
  for (const auto &Entry : this->Stats) {
    SortedStats.emplace(Entry.second.DynamicInst, &Entry.second);
  }
  for (auto StatIter = SortedStats.rbegin(), StatEnd = SortedStats.rend();
       StatIter != StatEnd; ++StatIter) {
    StatIter->second->dumpStats(O);
  }
}