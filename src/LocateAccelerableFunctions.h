#ifndef LLVM_TDG_LOCATE_ACCELERABLE_FUNCTIONS_H
#define LLVM_TDG_LOCATE_ACCELERABLE_FUNCTIONS_H

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include <unordered_map>
#include <unordered_set>

// A analysis pass to locate all the accelerable functions.
// A function is accelerable if:
// 1. it has no call to external functions.
// 2. the function it calls is also acclerable.
// In short, we can trace these functions and that's why they are accelerable.
class LocateAccelerableFunctions : public llvm::ModulePass {
public:
  static char ID;
  LocateAccelerableFunctions() : llvm::ModulePass(ID) {}
  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
  bool runOnModule(llvm::Module &Module) override;
  bool isAccelerable(const std::string &FunctionName) const {
    return this->FunctionUnAccelerable.find(FunctionName) ==
           this->FunctionUnAccelerable.end();
  }

private:
  std::unordered_set<std::string> FunctionUnAccelerable;

  std::unordered_set<std::string> MathAcclerableFunctionNames;

  // Some special math function we always want to mark as accelerable,
  // even if we don't have the trace.
  bool isMathAccelerable(const std::string &FunctionName) {
    return this->MathAcclerableFunctionNames.find(FunctionName) !=
           this->MathAcclerableFunctionNames.end();
  }
};

class AccelerableFunctionInfo : public llvm::FunctionPass {
public:
  static char ID;
  AccelerableFunctionInfo() : llvm::FunctionPass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
  bool doInitialization(llvm::Module &Module) override;
  bool runOnFunction(llvm::Function &Function) override;
  bool doFinalization(llvm::Module &Module) override;

  // We want to collect some statistics.
  uint64_t FunctionCount;
  uint64_t AccelerableFunctionCount;
  uint64_t InstructionCount;
  uint64_t AccelerableInstructionCount;
  uint64_t TopLevelLoopCount;
  uint64_t AccelerableTopLevelLoopCount;
  uint64_t AllLoopCount;
  uint64_t AccelerableAllLoopCount;

private:
  uint64_t countInstructionInFunction(const llvm::Function *Function) const;
  void countLoopInCurrentFunction(uint64_t &TopLevelLoopCount,
                                  uint64_t &AllLoopCount);
};

#endif