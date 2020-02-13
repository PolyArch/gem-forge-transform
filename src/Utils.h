#ifndef LLVM_TDG_UTILS_H
#define LLVM_TDG_UTILS_H

#include "DynamicInstruction.h"
#include "trace/InstructionUIDMap.h"

#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instruction.h"

#include "google/protobuf/message.h"

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

  static const llvm::Value *getMemAddrValue(const llvm::LoadInst *Inst) {
    return Inst->getOperand(0);
  }

  static const llvm::Value *getMemAddrValue(const llvm::StoreInst *Inst) {
    return Inst->getOperand(1);
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

  static uint64_t getMemTypeSize(const DynamicInstruction *DynamicInst,
                                 llvm::DataLayout *DataLayout) {
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(Utils::isMemAccessInst(StaticInst) &&
           "This is not a memory access inst to get memory type size.");
    if (llvm::isa<llvm::StoreInst>(StaticInst)) {
      return DataLayout->getTypeStoreSize(StaticInst->getOperand(0)->getType());
    } else {
      return DataLayout->getTypeStoreSize(StaticInst->getType());
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
   * Get a global unique name for the function.
   * We do not use the name of the function directly as:
   * 1. In C++, mangled names sometimes are too long. And we want to use the
   *    name in file name (maximum 256) for clarity.
   * 2. Missing any source location information.
   *
   * We require that all bc are compiled with line tables. And we use:
   * Souce.Line.(Demangled)Name.
   * Name is removed if the name is too long.
   */
  static std::string formatLLVMFunc(const llvm::Function *Func);

  static std::string formatLLVMBB(const llvm::BasicBlock *BB) {
    return (llvm::Twine(Utils::formatLLVMFunc(BB->getParent())) +
            "::" + BB->getName())
        .str();
  }

  /**
   * Get a global unique name for the inst.
   * Func::BB::Name/PosInBB(Op).
   */
  static std::string formatLLVMInst(const llvm::Instruction *Inst) {
    if (Inst->getName() != "") {
      return (llvm::Twine(Utils::formatLLVMFunc(Inst->getFunction())) +
              "::" + Inst->getParent()->getName() + "::" + Inst->getName() +
              "(" + Inst->getOpcodeName() + ")")
          .str();
    } else {
      size_t Idx = Utils::getLLVMInstPosInBB(Inst);
      return (llvm::Twine(Utils::formatLLVMFunc(Inst->getFunction())) +
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

  static std::string formatLLVMValueWithoutFunc(const llvm::Value *Value) {
    if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
      return Utils::formatLLVMInstWithoutFunc(Inst);
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

  // Singleton InstUIDMap.
  static InstructionUIDMap &getInstUIDMap() { return Utils::InstUIDMap; }

  static void
  dumpProtobufMessageToJson(const ::google::protobuf::Message &Message,
                            const ::std::string &FileName);

  /**
   * Decode functions from FuncNames.
   * Used to get ROI functions from command line options.
   * Function names will be demangled.
   * Must be dot separated.
   */
  static std::unordered_set<llvm::Function *>
  decodeFunctions(std::string FuncNames, llvm::Module *Module);
  static constexpr char FuncNameSeparator = '|';

private:
  static InstructionUIDMap InstUIDMap;
};

#endif