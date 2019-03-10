#ifndef LLVM_TDG_SPECULATIVE_PRECOMPUTATION_CRITICAL_LOAD_MANAGER_H
#define LLVM_TDG_SPECULATIVE_PRECOMPUTATION_CRITICAL_LOAD_MANAGER_H

#include "speculative_precomputation/PrecomputationSlice.h"

#include "DataGraph.h"

#include "llvm/IR/Instructions.h"

#include <string>

class CriticalLoadManager {
public:
  CriticalLoadManager(llvm::LoadInst *_CriticalLoad,
                      const std::string &_RootPath);

  CriticalLoadManager(const CriticalLoadManager &Other) = delete;
  CriticalLoadManager &operator=(const CriticalLoadManager &Other) = delete;

  CriticalLoadManager(CriticalLoadManager &&Other) = delete;
  CriticalLoadManager &operator=(CriticalLoadManager &&Other) = delete;

  void hit(DataGraph *DG);
  void clear();

private:
  llvm::LoadInst *CriticalLoad;
  std::string AnalyzePath;

  /**
   * A simple state machine.
   *
   * The load will first be identified as critical (Unknown to Idle).
   * The first critical load triggers the RIB to Building.
   * The second critical load makes the RIB to Built.
   *
   * Unknown: the critical load has not be identified.
   * Idle: the load has been identitified, waiting for the first one.
   * Building: hit the critical load for the first time.
   * Built: the second time, the slice is constructed.
   */
  enum StatusE {
    Unknown,
    Idle,
    Building,
    Built,
  };

  StatusE Status;
};

#endif