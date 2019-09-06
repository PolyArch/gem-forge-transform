#include "stream/StreamPass.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Transforms/Utils/Cloning.h"

#define DEBUG_TYPE "StreamExecutionPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

namespace {

/**
 * This pass will actually clone the module and transform it using identified
 * stream. This is used for execution-driven simulation.
 */

class StreamExecutionPass : public StreamPass {
public:
  static char ID;
  StreamExecutionPass() : StreamPass(ID) {}

protected:
  void transformStream() override;

  /**
   * The trasnformed module.
   * Since the original module is heavily referenced by the datagraph, we clone
   * it and modify the new one.
   */
  std::unique_ptr<llvm::Module> ClonedModule;
  llvm::ValueToValueMapTy ClonedValueMap;

  /**
   * Since we are purely static trasnformation, we have to first select
   * StreamRegionAnalyzer as our candidate. Here I sort them by the number of
   * dynamic memory accesses and exclude overlapped regions.
   */
  std::vector<StreamRegionAnalyzer *> selectStreamRegionAnalyzers();
  void writeModule();
};

void StreamExecutionPass::transformStream() {

  // First we select the StreamRegionAnalyzer we want to use.
  LLVM_DEBUG(llvm::errs() << "StreamExecution: Select StreamRegionAnalyzer.\n");
  auto streamRegionAnalyzers = this->selectStreamRegionAnalyzers();

  // Copy the module.
  LLVM_DEBUG(llvm::errs() << "StreamExecution: Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(*(this->Module), this->ClonedValueMap);

  // Generate the module.
  LLVM_DEBUG(llvm::errs() << "StreamExecution: Write the module.\n");
  this->writeModule();
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

void StreamExecutionPass::writeModule() {
  auto ModuleBCPath = this->OutputExtraFolderPath + "/stream.ex.bc";
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(ModuleBCPath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  assert(!ModuleFStream.has_error() &&
         "Failed to open the cloned module bc file.");
  llvm::WriteBitcodeToFile(*this->ClonedModule, ModuleFStream);
  ModuleFStream.close();

  if (GemForgeOutputDataGraphTextMode) {
    /**
     * Write to text mode for debug purpose.
     */
    auto ModuleLLPath = this->OutputExtraFolderPath + "/stream.ex.ll";
    std::error_code EC;
    llvm::raw_fd_ostream ModuleFStream(ModuleLLPath, EC,
                                       llvm::sys::fs::OpenFlags::F_None);
    assert(!ModuleFStream.has_error() &&
           "Failed to open the cloned module ll file.");
    this->ClonedModule->print(ModuleFStream, nullptr);
    ModuleFStream.close();
  }
}

} // namespace

#undef DEBUG_TYPE

char StreamExecutionPass::ID = 0;
static llvm::RegisterPass<StreamExecutionPass>
    X("stream-execution-pass", "Stream execution transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);