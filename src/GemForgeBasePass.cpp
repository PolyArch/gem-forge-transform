#include "GemForgeBasePass.h"

#include "Utils.h"

llvm::cl::opt<std::string> InstUIDFileName(
    "gem-forge-inst-uid-file",
    llvm::cl::desc("Inst UID Map file for datagraph building."));
llvm::cl::opt<std::string> GemForgeOutputExtraFolderPath(
    "output-extra-folder-path",
    llvm::cl::desc("Output extra information folder path, excluding the /"));
llvm::cl::opt<std::string> GemForgeROIFunctionNames(
    "gem-forge-roi-function",
    llvm::cl::desc("ROI function names. For C functions, just the name. For "
                   "C++ Functions, the signature is required. Dot separated."));

#define DEBUG_TYPE "GemForgeBasePass"

GemForgeBasePass::GemForgeBasePass(char _ID)
    : llvm::ModulePass(_ID), OutputExtraFolderPath("llvm.tdg.extra") {}
GemForgeBasePass::~GemForgeBasePass() {}

void GemForgeBasePass::getAnalysisUsage(llvm::AnalysisUsage &Info) const {
  Info.addRequired<llvm::TargetLibraryInfoWrapperPass>();
  Info.addRequired<llvm::AssumptionCacheTracker>();
}

bool GemForgeBasePass::initialize(llvm::Module &Module) {
  this->Module = &Module;

  // Initialize the InstUIDMap.
  assert(InstUIDFileName.getNumOccurrences() > 0 &&
         "You must provide instruction uid file.");
  Utils::getInstUIDMap().parseFrom(InstUIDFileName.getValue(), this->Module);

  this->CachedLI = new CachedLoopInfo(this->Module);
  this->CachedPDF = new CachedPostDominanceFrontier();
  this->CachedBBBranchDG = new CachedBBBranchDataGraph();
  this->CachedLU = new CachedLoopUnroller();

  if (::GemForgeOutputExtraFolderPath.getNumOccurrences() == 1) {
    this->OutputExtraFolderPath = ::GemForgeOutputExtraFolderPath.getValue();
  }

  if (GemForgeROIFunctionNames.getNumOccurrences() == 1) {
    this->ROIFunctions = Utils::decodeFunctions(
        GemForgeROIFunctionNames.getValue(), this->Module);
    // Make sure StreamMemIntrinsic is considered as ROI, if presented.
    if (auto StreamMemsetFunc = this->Module->getFunction("stream_memset")) {
      this->ROIFunctions.insert(StreamMemsetFunc);
    }
    if (auto StreamMemcpyFunc = this->Module->getFunction("stream_memcpy")) {
      this->ROIFunctions.insert(StreamMemcpyFunc);
    }
    if (auto StreamMemmoveFunc = this->Module->getFunction("stream_memmove")) {
      this->ROIFunctions.insert(StreamMemmoveFunc);
    }
  }

  return true;
}

bool GemForgeBasePass::finalize(llvm::Module &Module) {
  // Release the cached static loops.
  delete this->CachedLU;
  this->CachedLU = nullptr;
  delete this->CachedBBBranchDG;
  this->CachedBBBranchDG = nullptr;
  delete this->CachedPDF;
  this->CachedPDF = nullptr;
  delete this->CachedLI;
  this->CachedLI = nullptr;
  this->Module = nullptr;
  return true;
}

#undef DEBUG_TYPE
char GemForgeBasePass::ID = 0;