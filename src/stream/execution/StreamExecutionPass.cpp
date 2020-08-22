#include "StreamExecutionPass.h"

#include "StreamExecutionTransformer.h"

#define DEBUG_TYPE "StreamExecutionPass"

void StreamExecutionPass::transformStream() {
  // First we select the StreamRegionAnalyzer we want to use.
  LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer.\n");
  auto SelectedStreamRegionAnalyzers = this->selectStreamRegionAnalyzers();

  // Use the transformer to transform the regions.
  StreamExecutionTransformer(this->Module, this->ROIFunctions, this->CachedLI,
                             this->OutputExtraFolderPath,
                             GemForgeOutputDataGraphTextMode,
                             SelectedStreamRegionAnalyzers);

  // So far we still need to generate the history for testing purpose.
  // TODO: Remove this once we are done.
  StreamPass::transformStream();
}

std::vector<StreamRegionAnalyzer *>
StreamExecutionPass::selectStreamRegionAnalyzers() {
  std::vector<StreamRegionAnalyzer *> AllRegions;

  AllRegions.reserve(this->LoopStreamAnalyzerMap.size());
  for (auto &LoopStreamAnalyzerPair : this->LoopStreamAnalyzerMap) {
    AllRegions.push_back(LoopStreamAnalyzerPair.second.get());
  }

  // Sort them by the number of dynamic memory accesses.
  std::sort(
      AllRegions.begin(), AllRegions.end(),
      [](const StreamRegionAnalyzer *A, const StreamRegionAnalyzer *B) -> bool {
        return A->getNumDynamicMemAccesses() > B->getNumDynamicMemAccesses();
      });

  // Remove overlapped regions.
  std::vector<StreamRegionAnalyzer *> NonOverlapRegions;
  for (auto Region : AllRegions) {
    bool Overlapped = false;
    auto TopLoop = Region->getTopLoop();
    for (auto SelectedRegion : NonOverlapRegions) {
      auto SelectedTopLoop = SelectedRegion->getTopLoop();
      if (TopLoop->contains(SelectedTopLoop) ||
          SelectedTopLoop->contains(TopLoop)) {
        Overlapped = true;
        break;
      }
    }
    if (!Overlapped) {
      LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer "
                              << LoopUtils::getLoopId(TopLoop) << ".\n");
      NonOverlapRegions.push_back(Region);
    }
  }
  return NonOverlapRegions;
}

char StreamExecutionPass::ID = 0;
static llvm::RegisterPass<StreamExecutionPass>
    X("stream-execution-pass", "Stream execution transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);