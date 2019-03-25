#include "Utils.h"

#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "LLVM_TDG_UTILS"

std::unordered_map<const llvm::Function *, std::string>
    Utils::MemorizedDemangledFunctionNames;

std::unordered_map<const llvm::Instruction *, const llvm::Instruction *>
    Utils::MemorizedStaticNextNonPHIInstMap;

const std::string &Utils::getDemangledFunctionName(const llvm::Function *Func) {
  auto MemorizedIter = Utils::MemorizedDemangledFunctionNames.find(Func);
  if (MemorizedIter == Utils::MemorizedDemangledFunctionNames.end()) {

    size_t BufferSize = 1024;
    char *Buffer = static_cast<char *>(std::malloc(BufferSize));
    std::string MangledName = Func->getName();
    int DemangleStatus;
    size_t DemangledNameSize = BufferSize;
    auto DemangledName = llvm::itaniumDemangle(
        MangledName.c_str(), Buffer, &DemangledNameSize, &DemangleStatus);

    if (DemangledName == nullptr) {
      DEBUG(llvm::errs() << "Failed demangling name " << MangledName
                         << " due to ");
      switch (DemangleStatus) {
      case -4: {
        DEBUG(llvm::errs() << "unknown error.\n");
        break;
      }
      case -3: {
        DEBUG(llvm::errs() << "invalid args.\n");
        break;
      }
      case -2: {
        DEBUG(llvm::errs() << "invalid mangled name.\n");
        break;
      }
      case -1: {
        DEBUG(llvm::errs() << "memory alloc failure.\n");
        break;
      }
      default: { llvm_unreachable("Illegal demangle status."); }
      }
      // When failed, we return the original name.
      MemorizedIter =
          Utils::MemorizedDemangledFunctionNames.emplace(Func, MangledName)
              .first;

    } else {
      // Succeeded.
      std::string DemangledNameStr(DemangledName);
      auto Pos = DemangledNameStr.find('(');
      if (Pos != std::string::npos) {
        DemangledNameStr = DemangledNameStr.substr(0, Pos);
      }
      MemorizedIter =
          Utils::MemorizedDemangledFunctionNames.emplace(Func, DemangledNameStr)
              .first;
    }

    if (DemangledName != Buffer || DemangledNameSize >= BufferSize) {
      // Realloc happened.
      BufferSize = DemangledNameSize;
      Buffer = DemangledName;
    }

    // Deallocate the buffer.
    std::free(Buffer);
  }

  return MemorizedIter->second;
}

const llvm::Instruction *
Utils::getStaticNextNonPHIInst(const llvm::Instruction *Inst) {

  if (MemorizedStaticNextNonPHIInstMap.count(Inst) == 0) {
    const llvm::Instruction *StaticNextNonPHIInst = nullptr;
    const auto BB = Inst->getParent();
    if (Inst->isTerminator()) {
      const auto Func = BB->getParent();
      const llvm::BasicBlock *NextBB = nullptr;
      for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
           ++BBIter) {
        if ((&*BBIter) == BB) {
          ++BBIter;
          if (BBIter != BBEnd) {
            NextBB = &*BBIter;
          }
          break;
        }
      }
      if (NextBB != nullptr) {
        // Use the first non-phi instructions.
        StaticNextNonPHIInst = NextBB->getFirstNonPHI();
      }
    } else {
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        if ((&*InstIter) == Inst) {
          ++InstIter;
          assert(InstIter != InstEnd && "Non-Terminator at the end of BB.");
          StaticNextNonPHIInst = &*InstIter;
        }
      }
      assert(StaticNextNonPHIInst != nullptr &&
             "Failed to find the inst in the BB.");
    }
    MemorizedStaticNextNonPHIInstMap.emplace(Inst, StaticNextNonPHIInst);
  }

  return MemorizedStaticNextNonPHIInstMap.at(Inst);
}

bool Utils::isAsmBranchInst(const llvm::Instruction *Inst) {
  switch (Inst->getOpcode()) {
  case llvm::Instruction::Call:
  case llvm::Instruction::Invoke: {
    const llvm::Value *CalleeValue = Utils::getCalledValue(Inst);
    if (llvm::isa<llvm::InlineAsm>(CalleeValue)) {
      return false;
    }
    if (auto Function = llvm::dyn_cast<llvm::Function>(CalleeValue)) {
      // Direct function call. Check if we have the definition.
      if (Function->isIntrinsic()) {
        return false;
      } else if (Function->isDeclaration()) {
        // Untraced function.
        return false;
      } else {
        return true;
      }
    } else {
      // Indirect function call.
      return true;
    }
  }
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return true;
  }
  case llvm::Instruction::Br: {
    auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(Inst);
    if (BranchInst->isConditional()) {
      return true;
    }
    /**
     * For unconditional branch, we check the static next instruction.
     * If it is the same as the successor, then we assume this branch
     * will be removed in the asm.
     */
    auto StaticNextNonPHIInst = Utils::getStaticNextNonPHIInst(Inst);
    if (StaticNextNonPHIInst == nullptr) {
      return true;
    }
    if (BranchInst->getSuccessor(0) != StaticNextNonPHIInst->getParent()) {
      return true;
    }
    return false;
  }
  default: { return false; }
  }
}

bool Utils::isAsmConditionalBranchInst(const llvm::Instruction *Inst) {
  assert(Utils::isAsmBranchInst(Inst) &&
         "isAsmConditionalBranchInst should only be called on AsmBranchInst.");
  switch (Inst->getOpcode()) {
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Invoke:
  case llvm::Instruction::Call:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return false;
  }
  case llvm::Instruction::Br: {
    auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(Inst);
    return BranchInst->isConditional();
  }
  default: { return false; }
  }
}

bool Utils::isAsmIndirectBranchInst(const llvm::Instruction *Inst) {
  assert(Utils::isAsmBranchInst(Inst) &&
         "isAsmIndirectBranchInst should only be called on AsmBranchInst.");
  switch (Inst->getOpcode()) {
  case llvm::Instruction::Br: {
    return false;
  }
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return true;
  }
  case llvm::Instruction::Invoke:
  case llvm::Instruction::Call: {
    auto CalleeValue = Utils::getCalledValue(Inst);
    return !llvm::isa<llvm::Function>(CalleeValue);
  }

  default: { return false; }
  }
}

#undef DEBUG_TYPE