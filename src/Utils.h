#ifndef LLVM_TDG_UTILS_H
#define LLVM_TDG_UTILS_H

#include "DynamicInstruction.h"

#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Instruction.h"

class Utils {
public:
  static constexpr uint64_t CacheLineSize = 64;
  static constexpr uint64_t CacheLineMask = ~((1ul << 6) - 1);

  static uint64_t getCacheLineAddr(uint64_t Addr) {
    return Addr & CacheLineMask;
  }

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

  static const llvm::Value *getCalledValue(const llvm::Instruction *Inst) {
    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      return CallInst->getCalledValue();
    } else if (auto InvokeInst = llvm::dyn_cast<llvm::InvokeInst>(Inst)) {
      return InvokeInst->getCalledValue();
    } else {
      llvm_unreachable(
          "Should not send no call/invoke instruction to getCalledFunction.");
    }
  }

  static const llvm::Function *
  getCalledFunction(const llvm::Instruction *Inst) {
    return llvm::dyn_cast<llvm::Function>(Utils::getCalledValue(Inst));
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

  static std::string formatLLVMInstWithoutFunc(const llvm::Instruction *Inst) {
    if (Inst->getName() != "") {
      return (llvm::Twine(Inst->getParent()->getName()) +
              "::" + Inst->getName() + "(" + Inst->getOpcodeName() + ")")
          .str();
    } else {
      size_t Idx = Utils::getLLVMInstPosInBB(Inst);
      return (llvm::Twine(Inst->getParent()->getName()) +
              "::" + llvm::Twine(Idx) + "(" + Inst->getOpcodeName() + ")")
          .str();
    }
  }

  static const std::string &
  getDemangledFunctionName(const llvm::Function *Func);

  static std::unordered_map<const llvm::Function *, std::string>
      MemorizedDemangledFunctionNames;

  static std::unordered_map<const llvm::Instruction *,
                            const llvm::Instruction *>
      MemorizedStaticNextNonPHIInstMap;

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

  /**
   * Get statically the next instruction.
   */
  static const llvm::Instruction *
  getStaticNextNonPHIInst(const llvm::Instruction *Inst);

  static std::string formatLLVMValue(const llvm::Value *Value) {
    if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
      return Utils::formatLLVMInst(Inst);
    } else {
      return Value->getName();
    }
  }

  /**
   * Split a string like a|b|c| into [a, b, c].
   */
  static std::vector<std::string> splitByChar(const std::string &source,
                                              char split) {
    std::vector<std::string> ret;
    size_t idx = 0, prev = 0;
    for (; idx < source.size(); ++idx) {
      if (source[idx] == split) {
        ret.push_back(source.substr(prev, idx - prev));
        prev = idx + 1;
      }
    }
    // Don't miss the possible last field.
    if (prev < idx) {
      ret.push_back(source.substr(prev, idx - prev));
    }
    return ret;
  }

  /**
   * Helper function to estimate the branching property of the asm from the llvm
   * instruction.
   *
   * br: (depending on the target) & direct.
   * indirectbr: unconditional & indirect.
   * swtich: assuming jump table implementation, unconditional & indirect.
   * call/invoke: unconditional & (depending on the callee).
   * ret: unconditional & indirect.
   */
  static bool isAsmBranchInst(const llvm::Instruction *Inst);
  static bool isAsmConditionalBranchInst(const llvm::Instruction *Inst);
  static bool isAsmIndirectBranchInst(const llvm::Instruction *Inst);
};

#endif