#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

namespace {
// The separator has to be something else than ',()*&:[a-z][A-Z][0-9]_', which
// will appear in the C++ function signature.
const char TraceFunctionNameSeparator = '.';
} // namespace

#define DEBUG_TYPE "StripM5Pass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

namespace {

bool isM5FunctionName(const std::string &FunctionName) {
  if (FunctionName.find("m5_") == 0) {
    // Hack: return true if the name start with "m5_".
    return true;
  } else {
    return false;
  }
}

// This is a pass to strip away definition of M5 pseudo insts.
class StripM5Pass : public llvm::FunctionPass {
public:
  static char ID;
  StripM5Pass() : llvm::FunctionPass(ID) {}
  bool doInitialization(llvm::Module &Module) override {
    this->Module = &Module;
    this->DataLayout = new llvm::DataLayout(this->Module);
    return false;
  }

  bool runOnFunction(llvm::Function &Function) override {
    auto FunctionName = Function.getName().str();
    LLVM_DEBUG(llvm::dbgs() << "Processing Function: " << FunctionName << '\n');

    if (isM5FunctionName(FunctionName)) {
      llvm::errs() << "Found " << FunctionName << ".\n";
      Function.deleteBody();
      return true;
    }

    return false;
  }

  bool doFinalization(llvm::Module &Module) override {
    delete this->DataLayout;
    this->DataLayout = nullptr;
    return false;
  }

private:
  llvm::Module *Module;
  llvm::DataLayout *DataLayout;
};

} // namespace

#undef DEBUG_TYPE

char StripM5Pass::ID = 0;
static llvm::RegisterPass<StripM5Pass>
    X("strip-m5-pass", "strip m5 pseudo inst pass", false, false);
