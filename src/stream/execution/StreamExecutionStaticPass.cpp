#include "StreamExecutionStaticPass.h"

#include "StreamExecutionTransformer.h"

#define DEBUG_TYPE "StreamExecutionStaticPass"

bool StreamExecutionStaticPass::runOnModule(llvm::Module &Module) {
  this->initialize(Module);
  LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer.\n");
  auto SelectedStreamRegionAnalyzers = this->selectStreamRegionAnalyzers();

  // Use the transformer to transform the regions.
  StreamExecutionTransformer(this->Module, this->OutputExtraFolderPath,
                             GemForgeOutputDataGraphTextMode,
                             SelectedStreamRegionAnalyzers);

  for (auto &Analyzer : SelectedStreamRegionAnalyzers) {
    // Fake the finalize CoalesceInfo.
    // Since there is no step, no stream will be coalesced.
    Analyzer->getFuncSE()->finalizeCoalesceInfo();

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
}

bool StreamExecutionStaticPass::finalize(llvm::Module &Module) {
  delete this->DataLayout;
  this->DataLayout = nullptr;
  GemForgeBasePass::finalize(Module);
}

std::vector<StreamRegionAnalyzer *>
StreamExecutionStaticPass::selectStreamRegionAnalyzers() {
  std::vector<StreamRegionAnalyzer *> Analyzers;

  // Search through all ROI functions.
  for (auto Func : this->ROIFunctions) {
    auto LI = this->CachedLI->getLoopInfo(Func);
    llvm::Loop *PrevAnalyzedLoop = nullptr;
    for (auto Loop : LI->getLoopsInPreorder()) {
      if (PrevAnalyzedLoop && PrevAnalyzedLoop->contains(Loop)) {
        // Skip this as it's already contained by other analyzer.
        continue;
      }
      if (LoopUtils::isLoopContinuous(Loop)) {
        // Check if loop is continuous.
        auto Analyzer = new StreamRegionAnalyzer(
            Analyzers.size(), this->CachedLI, Loop, this->DataLayout,
            this->OutputExtraFolderPath);

        // Directly call endRegion.
        // This will select streams, and build transform plan.
        Analyzer->endRegion(StreamPassQualifySeedStrategyE::STATIC,
                            StreamPassChooseStrategy);

        Analyzers.push_back(Analyzer);

        PrevAnalyzedLoop = Loop;
      }
    }
  }
  return Analyzers;
}

char StreamExecutionStaticPass::ID = 0;
static llvm::RegisterPass<StreamExecutionStaticPass>
    X("stream-execution-static-pass", "Stream execution static transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);