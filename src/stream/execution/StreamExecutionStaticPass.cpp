#include "StreamExecutionStaticPass.h"

#include "StreamExecutionTransformer.h"

#define DEBUG_TYPE "StreamExecutionStaticPass"

bool StreamExecutionStaticPass::runOnModule(llvm::Module &Module) {
  this->initialize(Module);
  LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer.\n");
  auto SelectedStreamRegionAnalyzers = this->selectStreamRegionAnalyzers();

  // Use the transformer to transform the regions.
  StreamExecutionTransformer(
      this->Module, this->CachedLI, this->OutputExtraFolderPath,
      GemForgeOutputDataGraphTextMode, SelectedStreamRegionAnalyzers);

  for (auto &Analyzer : SelectedStreamRegionAnalyzers) {
    /**
     * Normally we would call Analyzer->getFuncSE()->finalizeCoalesceInfo().
     * However, there is no trace, so we identify coalesced streams in
     * StreamExecutionTransformer.
     */
    // Analyzer->getFuncSE()->finalizeCoalesceInfo();

    // End transformation.
    // This will make sure the info are dumped.
    Analyzer->endTransform();
  }

  // Remember to free the analyzer.
  for (auto &Analyzer : SelectedStreamRegionAnalyzers) {
    delete Analyzer;
    Analyzer = nullptr;
  }
  SelectedStreamRegionAnalyzers.clear();

  this->finalize(Module);
  return false;
}

bool StreamExecutionStaticPass::initialize(llvm::Module &Module) {
  GemForgeBasePass::initialize(Module);
  this->DataLayout = new llvm::DataLayout(this->Module);
  return true;
}

bool StreamExecutionStaticPass::finalize(llvm::Module &Module) {
  delete this->DataLayout;
  this->DataLayout = nullptr;
  GemForgeBasePass::finalize(Module);
  return true;
}

std::vector<StreamRegionAnalyzer *>
StreamExecutionStaticPass::selectStreamRegionAnalyzers() {
  std::vector<StreamRegionAnalyzer *> Analyzers;

  // Search through all ROI functions.
  for (auto Func : this->ROIFunctions) {
    LLVM_DEBUG(llvm::errs() << "Searching " << Func->getName() << '\n');
    auto LI = this->CachedLI->getLoopInfo(Func);
    llvm::Loop *PrevAnalyzedLoop = nullptr;
    for (auto Loop : LI->getLoopsInPreorder()) {
      if (PrevAnalyzedLoop && PrevAnalyzedLoop->contains(Loop)) {
        // Skip this as it's already contained by other analyzer.
        continue;
      }
      LLVM_DEBUG(llvm::errs()
                 << "Processing loop " << LoopUtils::getLoopId(Loop) << '\n');
      if (LoopUtils::isLoopContinuous(Loop)) {
        // Check if loop is continuous.
        auto Analyzer = new StreamRegionAnalyzer(
            Analyzers.size(), this->CachedLI, this->CachedPDF,
            this->CachedBBPredDG, Loop, this->DataLayout,
            this->OutputExtraFolderPath);

        // Directly call endRegion.
        // This will select streams, and build transform plan.
        Analyzer->endRegion(StreamPassQualifySeedStrategyE::STATIC,
                            StreamPassChooseStrategy);

        Analyzers.push_back(Analyzer);

        PrevAnalyzedLoop = Loop;
      } else {
        LLVM_DEBUG(llvm::errs() << "Loop not continuous "
                                << LoopUtils::getLoopId(Loop) << '\n');
      }
    }
  }
  return Analyzers;
}

char StreamExecutionStaticPass::ID = 0;
static llvm::RegisterPass<StreamExecutionStaticPass>
    X("stream-execution-static-pass", "Stream execution static transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);