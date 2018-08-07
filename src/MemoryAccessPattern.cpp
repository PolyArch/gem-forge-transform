#include "MemoryAccessPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "MemoryAccessPattern"

void MemoryAccessPattern::addAccess(llvm::Instruction *Inst, uint64_t Addr) {
  auto Iter = this->ComputingPatternMap.find(Inst);
  if (Iter == this->ComputingPatternMap.end()) {
    Iter = this->ComputingPatternMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                        std::forward_as_tuple(Addr))
               .first;

    // TODO: Handle indirect memory access here.
    auto IndirectBase = this->isIndirect(Inst);
    if (IndirectBase != nullptr) {
      auto &Pattern = Iter->second;
      Pattern.CurrentPattern = INDIRECT;
      Pattern.BaseLoad = IndirectBase;
    }

    return;
  }
  auto &Pattern = Iter->second;
  Pattern.Count++;
  switch (Pattern.CurrentPattern) {
  case CONSTANT: {
    if (Addr == Pattern.Base) {
      // We are still accessing the same address.
    } else {
      // If this is the second time, switch to LINEAR; Otherwise, this is
      // Quadric.
      if (Pattern.Count == 2) {
        Pattern.CurrentPattern = LINEAR;
        Pattern.StrideI = Addr - Pattern.Base;
        Pattern.I = 1;
      } else {
        Pattern.CurrentPattern = QUARDRIC;
        Pattern.StrideI = 0;
        Pattern.I = 0;
        Pattern.NI = Pattern.Count;
        Pattern.StrideJ = Addr - Pattern.Base;
        Pattern.J = 1;
      }
    }
    break;
  }
  case LINEAR: {
    auto CurrentAddr = Pattern.Base + Pattern.StrideI * (Pattern.I + 1);
    if (Addr == CurrentAddr) {
      // We are still accessing the same address.
      Pattern.I++;
    } else {
      // Switch to quadric.
      Pattern.CurrentPattern = QUARDRIC;
      Pattern.NI = Pattern.I + 1;
      Pattern.I = 0;
      Pattern.StrideJ = Addr - Pattern.Base;
      Pattern.J = 1;
    }
    break;
  }
  case QUARDRIC: {
    // Have not reached the limit.
    auto CurrentAddr = Pattern.Base + Pattern.StrideJ * Pattern.J +
                       Pattern.StrideI * (Pattern.I + 1);
    if (Pattern.I == Pattern.NI) {
      // Reached the limit.
      CurrentAddr = Pattern.Base + Pattern.StrideJ * (Pattern.J + 1);
    }

    if (Addr == CurrentAddr) {
      // We are in the same pattern, update the index.
      Pattern.I++;
      if (Pattern.I == Pattern.NI) {
        Pattern.I = 0;
        Pattern.J++;
      }
    } else {
      // This is not what we can handle, switch to random.
      Pattern.CurrentPattern = RANDOM;
    }
    break;
  }
  case RANDOM: {
    // Nothing we can do now.
    break;
  }
  case INDIRECT: {
    // Indirect memory access is handled when the stream is first constructed.
    // Nothing we can do now.
    break;
  }
  default: {
    llvm_unreachable("Illegal memory access pattern.");
    break;
  }
  }
}

void MemoryAccessPattern::endLoop(llvm::Instruction *Inst) {
  auto Iter = this->ComputingPatternMap.find(Inst);
  assert(Iter != this->ComputingPatternMap.end() &&
         "This instruction's access pattern is not being computed.");
  Iter->second.StreamCount = 1;
  const auto &NewPattern = Iter->second;
  auto ComputedIter = this->ComputedPatternMap.find(Inst);
  if (ComputedIter == this->ComputedPatternMap.end()) {
    this->ComputedPatternMap.emplace(Inst, NewPattern);
  } else {
    auto &ComputedPattern = ComputedIter->second;

    if (ComputedPattern.CurrentPattern != NewPattern.CurrentPattern) {
      DEBUG(llvm::errs() << "Changing pattern.\n");
    }
    ComputedPattern.CurrentPattern = NewPattern.CurrentPattern;

    // Merge the result.
    ComputedPattern.Count += NewPattern.Count;
    ComputedPattern.StreamCount += NewPattern.StreamCount;
  }

  this->ComputingPatternMap.erase(Iter);
}

const MemoryAccessPattern::PatternComputation &
MemoryAccessPattern::getPattern(llvm::Instruction *Inst) const {
  // We are not sure about the pattern.
  auto Iter = this->ComputedPatternMap.find(Inst);
  if (Iter == this->ComputedPatternMap.end()) {
    assert(false &&
           "Failed getting memory access pattern for static instruction.");
  }
  return Iter->second;
}

std::string MemoryAccessPattern::formatPattern(Pattern Pat) {
  switch (Pat) {
  case RANDOM: {
    return "RANDOM";
  }
  case INDIRECT: {
    return "INDIRECT";
  }
  case CONSTANT: {
    return "CONSTANT";
  }
  case LINEAR: {
    return "LINEAR";
  }
  case QUARDRIC: {
    return "QUARDRIC";
  }
  default: { llvm_unreachable("Illegal pattern to be converted to string."); }
  }
}

llvm::Instruction *
MemoryAccessPattern::isIndirect(llvm::Instruction *Inst) const {
  bool IsLoad = llvm::isa<llvm::LoadInst>(Inst);
  llvm::Value *Addr = nullptr;
  if (IsLoad) {
    Addr = Inst->getOperand(0);
  } else {
    Addr = Inst->getOperand(1);
  }
  if (llvm::isa<llvm::Instruction>(Addr)) {
    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Addr)) {
      return LoadInst;
    }
    if (auto GEPInst = llvm::dyn_cast<llvm::GetElementPtrInst>(Addr)) {
      // Search one more level.
      size_t LoadCount = 0;
      llvm::Instruction *LoadInst = nullptr;
      for (unsigned Idx = 0, NumOperands = GEPInst->getNumOperands();
           Idx < NumOperands; ++Idx) {
        auto Operand = GEPInst->getOperand(Idx);
        if (auto OperandInst = llvm::dyn_cast<llvm::LoadInst>(Operand)) {
          LoadCount++;
          LoadInst = OperandInst;
        }
      }
      if (LoadCount == 1) {
        return LoadInst;
      }
    }
  }
  return nullptr;
}

#undef DEBUG_TYPE