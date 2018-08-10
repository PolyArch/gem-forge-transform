#include "MemoryAccessPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "MemoryAccessPattern"

bool MemoryAccessPattern::isPatternRelaxed(Pattern A, Pattern B) {
  switch (A) {
  case UNKNOWN: {
    return true;
  }
  case INDIRECT: {
    return B == INDIRECT;
  }
  case CONSTANT: {
    switch (B) {
    case CONSTANT:
    case LINEAR:
    case QUARDRIC:
    case RANDOM: {
      return true;
    }
    default: { return false; }
    }
  }
  case LINEAR: {
    switch (B) {
    case LINEAR:
    case QUARDRIC:
    case RANDOM: {
      return true;
    }
    default: { return false; }
    }
  }
  case QUARDRIC: {
    switch (B) {
    case QUARDRIC:
    case RANDOM: {
      return true;
    }
    default: { return false; }
    }
  }
  case RANDOM: {
    return B == RANDOM;
  }
  default: {
    llvm_unreachable("Illegal memory access pattern.");
    return false;
  }
  }
}

void MemoryAccessPattern::addMissingAccess(llvm::Instruction *Inst) {
  auto Iter = this->ComputingPatternMap.find(Inst);
  if (Iter == this->ComputingPatternMap.end()) {
    Iter = this->ComputingPatternMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                        std::forward_as_tuple())
               .first;

    assert(Iter->second.CurrentPattern == UNKNOWN &&
           "Missing access for the first time should be marked as unknown.");

    // TODO: Handle indirect memory access here.
    auto &Pattern = Iter->second;
    auto IndirectBase = this->isIndirect(Inst);
    if (IndirectBase != nullptr) {
      Pattern.CurrentPattern = INDIRECT;
      Pattern.BaseLoad = IndirectBase;
    }

    Pattern.Count = 0;

    return;
  }

  // Otherwise, implicitly assume it follows the current pattern, and update the
  // statistics and induction variable.
  auto &Pattern = Iter->second;
  Pattern.IsConditional = true;
  Pattern.Iters++;
  switch (Pattern.CurrentPattern) {
  case LINEAR: {
    Pattern.I++;
    break;
  }
  case QUARDRIC: {
    Pattern.J++;
    break;
  }
  default: { break; }
  }
  return;
}

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
  Pattern.Iters++;
  switch (Pattern.CurrentPattern) {
  case UNKNOWN: {
    // First time, go to CONSTANT.
    Pattern.CurrentPattern = CONSTANT;
    Pattern.Base = Addr;
    break;
  }
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
    if (Pattern.I + 1 == Pattern.NI) {
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

void MemoryAccessPattern::endStream(llvm::Instruction *Inst) {
  auto Iter = this->ComputingPatternMap.find(Inst);
  if (Iter == this->ComputingPatternMap.end()) {
    return;
  }
  const auto &NewPattern = Iter->second;
  auto ComputedIter = this->ComputedPatternMap.find(Inst);
  if (ComputedIter == this->ComputedPatternMap.end()) {
    this->ComputedPatternMap.emplace(Inst, NewPattern);
  } else {
    auto &ComputedPattern = ComputedIter->second;
    ComputedPattern.merge(NewPattern);
  }

  this->ComputingPatternMap.erase(Iter);
}

const MemoryAccessPattern::ComputedPattern &
MemoryAccessPattern::getPattern(llvm::Instruction *Inst) const {
  // We are not sure about the pattern.
  auto Iter = this->ComputedPatternMap.find(Inst);
  if (Iter == this->ComputedPatternMap.end()) {
    assert(false &&
           "Failed getting memory access pattern for static instruction.");
  }
  return Iter->second;
}

bool MemoryAccessPattern::contains(llvm::Instruction *Inst) const {
  return this->ComputedPatternMap.find(Inst) != this->ComputedPatternMap.end();
}

std::string MemoryAccessPattern::formatPattern(Pattern Pat) {
  switch (Pat) {
  case UNKNOWN: {
    return "UNKNOWN";
  }
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

void MemoryAccessPattern::ComputedPattern::merge(
    const PatternComputation &NewPattern) {
  this->Count += NewPattern.Count;
  this->Iters += NewPattern.Iters;
  this->StreamCount++;
  this->IsConditional = this->IsConditional || NewPattern.IsConditional;
  if (MemoryAccessPattern::isPatternRelaxed(this->CurrentPattern,
                                            NewPattern.CurrentPattern)) {
    this->CurrentPattern = NewPattern.CurrentPattern;
  }
}

void MemoryAccessPattern::finializePattern() {
  for (auto &Entry : this->ComputedPatternMap) {
    auto &Pattern = Entry.second;
    if (static_cast<float>(Pattern.Count) /
            static_cast<float>(Pattern.StreamCount) <
        3.0) {
      // In such case we believe this is not a stream.
      Pattern.CurrentPattern = RANDOM;
    }
  }
}

#undef DEBUG_TYPE