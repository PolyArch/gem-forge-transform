#ifndef LLVM_TDG_UTILS_H
#define LLVM_TDG_UTILS_H

#include "llvm/IR/Instruction.h"

class Utils {
public:
  static bool isCallOrInvokeInst(llvm::Value *Value) {
    return llvm::isa<llvm::CallInst>(Value) ||
           llvm::isa<llvm::InvokeInst>(Value);
  }

  static llvm::Function *getCalledFunction(llvm::Instruction *Inst) {
    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      return CallInst->getCalledFunction();
    } else if (auto InvokeInst = llvm::dyn_cast<llvm::InvokeInst>(Inst)) {
      return InvokeInst->getCalledFunction();
    } else {
      llvm_unreachable(
          "Should not send no call/invoke instruction to getCalledFunction.");
    }
  }
};

#endif