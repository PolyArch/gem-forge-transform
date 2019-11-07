#include "Replay.h"

#include "TransformUtils.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include <queue>
#include <unordered_set>

#define DEBUG_TYPE "ValidExecutionPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

namespace {

/**
 * This pass simply clones the module. This is used to represent a validation
 * build. We override runOnModule() to require only the module and extra path.
 */

class ValidExecutionPass : public ReplayTrace {
public:
  static char ID;
  ValidExecutionPass() : ReplayTrace(ID) {}

  bool runOnModule(llvm::Module &Module) override {
    this->initialize(Module);
    // Generate the module.
    this->transformModule();
    LLVM_DEBUG(llvm::errs() << "Write the module.\n");
    this->writeModule();
    this->finalize(Module);
    return false;
  }

protected:
  std::unique_ptr<llvm::Module> ClonedModule;
  llvm::ValueToValueMapTy ClonedValueMap;
  std::unique_ptr<CachedLoopInfo> ClonedCachedLI;
  std::unique_ptr<llvm::DataLayout> ClonedDataLayout;

  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;

  /**
   * Utility functions.
   */
  template <typename T>
  std::enable_if_t<std::is_base_of<llvm::Value, T>::value, T *>
  getClonedValue(const T *Value) {
    auto ClonedValueIter = this->ClonedValueMap.find(Value);
    if (ClonedValueIter == this->ClonedValueMap.end()) {
      llvm::errs() << "Failed to find cloned value "
                   << Utils::formatLLVMValue(Value) << '\n';
    }

    assert(ClonedValueIter != this->ClonedValueMap.end() &&
           "Failed to find cloned value.");
    auto ClonedValue = llvm::dyn_cast<T>(ClonedValueIter->second);
    assert(ClonedValue != nullptr && "Mismatched cloned value type.");
    return ClonedValue;
  }

  void transformModule();
  void writeModule();
};

bool ValidExecutionPass::initialize(llvm::Module &Module) {
  // We need the CachedLI.
  GemForgeBasePass::initialize(Module);
  LLVM_DEBUG(llvm::errs() << "My Module " << this->Module << " " << &Module
                          << "\n");
  // Copy the module.
  LLVM_DEBUG(llvm::errs() << "Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(Module, this->ClonedValueMap);
  this->ClonedDataLayout =
      std::make_unique<llvm::DataLayout>(this->ClonedModule.get());
  // Initialize the CachedLI for the cloned module.
  this->ClonedCachedLI =
      std::make_unique<CachedLoopInfo>(this->ClonedModule.get());
  return true;
}

bool ValidExecutionPass::finalize(llvm::Module &Module) {
  GemForgeBasePass::finalize(Module);
  return true;
}

void ValidExecutionPass::transformModule() {
  /**
   * We need to transform the module if we are doing region simpoint.
   */
  if (!GemForgeRegionSimpoint) {
    return;
  }
  // Load the region function and bb from the fake Trace file.
  std::ifstream RegionFile(TraceFileName.getValue());
  std::string RegionFuncName;
  std::string RegionBBName;
  RegionFile >> RegionFuncName >> RegionBBName;
  RegionFile.close();
  LLVM_DEBUG(llvm::errs() << "Select region " << RegionFuncName << ' '
                          << RegionBBName << '\n');
  // Identify the region within the cloned module.
  auto ClonedFunc = this->ClonedModule->getFunction(RegionFuncName);
  assert(ClonedFunc && "Failed to find region function.");
  auto ClonedLI = this->ClonedCachedLI->getLoopInfo(ClonedFunc);
  llvm::Loop *ClonedLoop = nullptr;
  for (auto BBIter = ClonedFunc->begin(), BBEnd = ClonedFunc->end();
       BBIter != BBEnd; ++BBIter) {
    if (BBIter->getName() == RegionBBName) {
      // Found the BB.
      // auto ClonedBB = this->getClonedValue(&*BBIter);
      auto ClonedBB = &*BBIter;
      ClonedLoop = ClonedLI->getLoopFor(ClonedBB);
      break;
    }
  }
  assert(ClonedLoop && "Failed to find the loop in cloned module.");
  LLVM_DEBUG(llvm::errs() << "Find the region in cloned module.");
  // Insert the function prototype.
  auto &Context = this->ClonedModule->getContext();
  auto VoidTy = llvm::Type::getVoidTy(Context);
  std::vector<llvm::Type *> GemForgeRegionSimpointArgs;
  auto GemForgeRegionSimpointTy =
      llvm::FunctionType::get(VoidTy, GemForgeRegionSimpointArgs, false);
  auto GemForgeRegionSimpointFunc =
      this->ClonedModule
          ->getOrInsertFunction("m5_gem_forge_region_simpoint",
                                GemForgeRegionSimpointTy)
          .getCallee();
  LLVM_DEBUG(llvm::errs() << "Insert m5_gem_forge_region_simpoint().\n");

  // Insert this to the preheader of the loop.
  auto ClonedPreheader = ClonedLoop->getLoopPredecessor();
  if (!ClonedPreheader) {
    LLVM_DEBUG(llvm::errs() << "Create preheader for loop " << RegionFuncName
                            << ' ' << RegionBBName << '\n');
    auto ClonedDT = this->ClonedCachedLI->getDominatorTree(ClonedFunc);
    // We pass LI and DT so that they are updated.
    ClonedPreheader = llvm::InsertPreheaderForLoop(ClonedLoop, ClonedDT,
                                                   ClonedLI, nullptr, false);
  }
  auto ClonedPreheaderTerminator = ClonedPreheader->getTerminator();
  llvm::IRBuilder<> Builder(ClonedPreheaderTerminator);
  Builder.CreateCall(GemForgeRegionSimpointFunc);
}

void ValidExecutionPass::writeModule() {
  auto ModuleBCPath = this->OutputExtraFolderPath + "/ex.bc";
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
    auto ModuleLLPath = this->OutputExtraFolderPath + "/ex.ll";
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

char ValidExecutionPass::ID = 0;
static llvm::RegisterPass<ValidExecutionPass>
    X("valid-execution-pass", "Valid execution transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);