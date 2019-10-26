#ifndef GEM_FORGE_PROFILE_ANALYZE_PASS_H
#define GEM_FORGE_PROFILE_ANALYZE_PASS_H

#include "GemForgeBasePass.h"

#include "trace/ProfileParser.h"

#include <ostream>

class ProfileAnalyzePass : public GemForgeBasePass {
public:
  static char ID;
  ProfileAnalyzePass();
  ~ProfileAnalyzePass() override;

  bool runOnModule(llvm::Module &Module) override;

protected:
  std::unique_ptr<ProfileParser> Profile;

  std::unordered_map<llvm::Loop *, uint64_t> LoopInstCountMap;
  std::vector<llvm::Loop *> SortedOuterMostLoops;
  uint64_t TotalInsts;

  /**
   * Get instruction count for a loop, return zero if not found.
   */
  uint64_t getLoopInstCount(llvm::Loop *Loop) const;

  void analyzeRegionInFunc(llvm::Function *Func);
  void sortOuterMostLoops();

  /**
   * Select the region-based simpoints.
   */
  std::vector<llvm::Loop *> SortedRegionSimpoints;
  void selectRegionSimpoints();

  void dumpTo() const;
  void dumpLoopTo(llvm::Loop *OuterMostLoop, std::ostream &O,
                  bool Expand = true) const;
};

#endif