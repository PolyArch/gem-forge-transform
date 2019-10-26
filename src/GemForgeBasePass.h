#ifndef GEM_FORGE_BASE_PASS_H
#define GEM_FORGE_BASE_PASS_H

#include "LoopUnroller.h"

#include "llvm/Pass.h"

extern llvm::cl::opt<std::string> InstUIDFileName;

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
  CachedLoopUnroller *CachedLU = nullptr;
};

#endif