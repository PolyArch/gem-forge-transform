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
 * build.
 */

class ValidExecutionPass : public ReplayTrace {
public:
  static char ID;
  ValidExecutionPass() : ReplayTrace(ID) {}

protected:
  void transform() override;

  std::unique_ptr<llvm::Module> ClonedModule;
  llvm::ValueToValueMapTy ClonedValueMap;

  void writeModule();
};

void ValidExecutionPass::transform() {

  // Copy the module.
  LLVM_DEBUG(llvm::errs() << "Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(*(this->Module), this->ClonedValueMap);
  // Generate the module.
  LLVM_DEBUG(llvm::errs() << "Write the module.\n");
  this->writeModule();
}

void ValidExecutionPass::writeModule() {
  auto ModuleBCPath = this->OutputExtraFolderPath + "/valid.ex.bc";
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
    auto ModuleLLPath = this->OutputExtraFolderPath + "/valid.ex.ll";
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