#ifndef LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PASS_H
#define LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PASS_H

#include "Replay.h"

#include "speculative_precomputation/CriticalLoadManager.h"

extern llvm::cl::opt<std::string> LoadProfileFileName;
/**
 * Implement a (dynamic) speculative precomputation pass.
 *
 * 1. Identify critical loads from the the run time profiling.
 * 2. Create the slices for the stream.
 *
 */

class SpeculativePrecomputationPass : public ReplayTrace {
public:
  static char ID;
  SpeculativePrecomputationPass() : ReplayTrace(ID) {}

protected:
  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;
  void transform() override;
  void dumpStats(std::ostream &O) override;

  std::unordered_map<const llvm::LoadInst *, CriticalLoadManager>
      InstManagerMap;

  void initializeCriticalLoadManagers();
};

#endif