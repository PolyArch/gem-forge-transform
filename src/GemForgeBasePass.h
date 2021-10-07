#ifndef GEM_FORGE_BASE_PASS_H
#define GEM_FORGE_BASE_PASS_H

#include "BasicBlockBranchDataGraph.h"
#include "LoopUnroller.h"

#include "llvm/Pass.h"

#include <unordered_set>

extern llvm::cl::opt<std::string> InstUIDFileName;
extern llvm::cl::opt<std::string> GemForgeOutputExtraFolderPath;
extern llvm::cl::opt<std::string> GemForgeROIFunctionNames;

/**
 * Served as the base pass for all GemForgePass.
 * Hold the required analysis result, e.g. LoopInfo.
 *
 * User should implement runOnModule() and call to
 * initialize() and finalize().
 */
class GemForgeBasePass : public llvm::ModulePass {
public:
  static char ID;
  GemForgeBasePass(char _ID);
  virtual ~GemForgeBasePass();

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;

protected:
  virtual bool initialize(llvm::Module &Module);
  virtual bool finalize(llvm::Module &Module);

  llvm::Module *Module = nullptr;
  CachedLoopInfo *CachedLI = nullptr;
  CachedPostDominanceFrontier *CachedPDF = nullptr;
  CachedBBBranchDataGraph *CachedBBBranchDG = nullptr;
  CachedLoopUnroller *CachedLU = nullptr;

  std::unordered_set<llvm::Function *> ROIFunctions;

  std::string OutputExtraFolderPath;
};

#endif