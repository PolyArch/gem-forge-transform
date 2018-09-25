#ifndef LLVM_TDG_UTILS_H
#define LLVM_TDG_UTILS_H

#include "DynamicInstruction.h"

#include "llvm/IR/Instruction.h"

class Utils {
public:
  static bool isMemAccessInst(const llvm::Instruction *Inst) {
    return llvm::isa<llvm::StoreInst>(Inst) || llvm::isa<llvm::LoadInst>(Inst);
  }

  static const llvm::Value *getMemAddrValue(const llvm::Instruction *Inst) {
    assert(
        Utils::isMemAccessInst(Inst) &&
        "This is not a memory access instruction to get memory address value.");
    if (llvm::isa<llvm::StoreInst>(Inst)) {
      return Inst->getOperand(1);
    } else {
      return Inst->getOperand(0);
    }
  }

  static uint64_t getMemAddr(const DynamicInstruction *DynamicInst) {
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(Utils::isMemAccessInst(StaticInst) &&
           "This is not a memory access inst to get address.");
    if (llvm::isa<llvm::StoreInst>(StaticInst)) {
      return DynamicInst->DynamicOperands[1]->getAddr();
    } else {
      return DynamicInst->DynamicOperands[0]->getAddr();
    }
  }

  static bool isCallOrInvokeInst(const llvm::Value *Value) {
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

  static llvm::Value *getArgOperand(llvm::Instruction *Inst, unsigned Idx) {

    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      return CallInst->getArgOperand(Idx);
    } else if (auto InvokeInst = llvm::dyn_cast<llvm::InvokeInst>(Inst)) {
      return InvokeInst->getArgOperand(Idx);
    } else {
      llvm_unreachable(
          "Should not send no call/invoke instruction to getArgOperand.");
    }
  }
  /**
   * Print an static instruction.
   */
  static std::string formatLLVMInst(const llvm::Instruction *Inst) {
    if (Inst->getName() != "") {
      return (llvm::Twine(Inst->getFunction()->getName()) +
              "::" + Inst->getParent()->getName() + "::" + Inst->getName() +
              "(" + Inst->getOpcodeName() + ")")
          .str();
    } else {
      size_t Idx = Utils::getLLVMInstPosInBB(Inst);
      return (llvm::Twine(Inst->getFunction()->getName()) +
              "::" + Inst->getParent()->getName() + "::" + llvm::Twine(Idx) +
              "(" + Inst->getOpcodeName() + ")")
          .str();
    }
  }

  static size_t getLLVMInstPosInBB(const llvm::Instruction *Inst) {
    size_t Idx = 0;
    for (auto InstIter = Inst->getParent()->begin(),
              InstEnd = Inst->getParent()->end();
         InstIter != InstEnd; ++InstIter) {
      if ((&*InstIter) == Inst) {
        break;
      }
      Idx++;
    }
    return Idx;
  }

  static std::string formatLLVMValue(const llvm::Value *Value) {
    if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
      return Utils::formatLLVMInst(Inst);
    } else {
      return Value->getName();
    }
  }
};

#endif