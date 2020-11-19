#ifndef GEM_FORGE_STREAM_EXECUTION_STATIC_PASS_H
#define GEM_FORGE_STREAM_EXECUTION_STATIC_PASS_H

#include "stream/StreamPass.h"

/**
 * This pass transform streams based on purely static information.
 */

class StreamExecutionStaticPass : public GemForgeBasePass {
public:
  static char ID;
  StreamExecutionStaticPass() : GemForgeBasePass(ID) {}

  bool runOnModule(llvm::Module &Module) override;

protected:
  llvm::DataLayout *DataLayout;

  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;

  std::vector<DynStreamRegionAnalyzer *> selectStreamRegionAnalyzers();
};

#endif
