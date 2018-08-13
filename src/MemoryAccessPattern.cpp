#include "MemoryAccessPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "MemoryAccessPattern"

void MemoryAccessPattern::AddressPatternFSM::update(uint64_t Addr) {
  this->PrevAddress = Addr;
  this->Updates++;
  switch (this->CurrentPattern) {
  case UNKNOWN: {
    // First time, go to CONSTANT.
    this->CurrentPattern = CONSTANT;
    this->Base = Addr;
    break;
  }
  case CONSTANT: {
    if (Addr == this->Base) {
      // We are still accessing the same address.
    } else {
      // If this is the second time, switch to LINEAR; Otherwise, this is
      // Quadric.
      if (this->Updates == 2) {
        this->CurrentPattern = LINEAR;
        this->StrideI = Addr - this->Base;
        this->I = 1;
      } else {
        this->CurrentPattern = QUARDRIC;
        this->StrideI = 0;
        this->I = 0;
        this->NI = this->Updates;
        this->StrideJ = Addr - this->Base;
        this->J = 1;
      }
    }
    break;
  }
  case LINEAR: {
    auto CurrentAddr = this->Base + this->StrideI * (this->I + 1);
    if (Addr == CurrentAddr) {
      // We are still accessing the same address.
      this->I++;
    } else {
      // Switch to quadric.
      this->CurrentPattern = QUARDRIC;
      this->NI = this->I + 1;
      this->I = 0;
      this->StrideJ = Addr - this->Base;
      this->J = 1;
    }
    break;
  }
  case QUARDRIC: {
    // Have not reached the limit.
    auto CurrentAddr =
        this->Base + this->StrideJ * this->J + this->StrideI * (this->I + 1);
    if (this->I + 1 == this->NI) {
      // Reached the limit.
      CurrentAddr = this->Base + this->StrideJ * (this->J + 1);
    }

    if (Addr == CurrentAddr) {
      // We are in the same pattern, update the index.
      this->I++;
      if (this->I == this->NI) {
        this->I = 0;
        this->J++;
      }
    } else {
      // This is not what we can handle, switch to random.
      this->CurrentPattern = RANDOM;
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

void MemoryAccessPattern::AddressPatternFSM::updateMissing() {
  // Implicitly assume it follows the current pattern, and update the
  // statistics and induction variable.
  this->Updates++;
  switch (this->CurrentPattern) {
  case LINEAR: {
    this->PrevAddress = this->Base + this->StrideI * (this->I + 1);
    this->I++;
    break;
  }
  case QUARDRIC: {
    if (this->I + 1 == this->NI) {
      // Reached the limit.
      this->PrevAddress = this->Base + this->StrideJ * (this->J + 1);
      this->I = 0;
      this->J++;
    } else {
      this->PrevAddress =
          this->Base + this->StrideJ * this->J + this->StrideI * (this->I + 1);
      this->I++;
    }

    break;
  }
  default: { break; }
  }
}

void MemoryAccessPattern::AccessPatternFSM::addMissingAccess() {
  this->Iters++;
  if (this->State == FAILURE) {
    return;
  }
  switch (this->AccPattern) {
  case MemoryAccessPattern::AccessPattern::UNCONDITIONAL: {
    this->State = FAILURE;
    return;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_ACCESS_ONLY: {
    this->AddressPattern.updateMissing();
    break;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_UPDATE_ONLY: {
    this->State = FAILURE;
    return;
  }
  case MemoryAccessPattern::AccessPattern::SAME_CONDITION: {
    break;
  }
  default: { llvm_unreachable("Illegal access pattern."); }
  }
  this->State = SUCCESS;
}

void MemoryAccessPattern::AccessPatternFSM::addAccess(uint64_t Addr) {
  this->Iters++;
  this->Accesses++;
  if (this->State == FAILURE) {
    return;
  }
  switch (this->AccPattern) {
  case MemoryAccessPattern::AccessPattern::UNCONDITIONAL: {
    this->AddressPattern.update(Addr);
    break;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_ACCESS_ONLY: {
    this->AddressPattern.update(Addr);
    break;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_UPDATE_ONLY: {
    if (Addr != this->AddressPattern.PrevAddress) {
      this->AddressPattern.update(Addr);
    }
    break;
  }
  case MemoryAccessPattern::AccessPattern::SAME_CONDITION: {
    this->AddressPattern.update(Addr);
    break;
  }
  }
  this->State = SUCCESS;
}

bool MemoryAccessPattern::isAddressPatternRelaxed(Pattern A, Pattern B) {
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
    llvm_unreachable("Illegal memory address pattern.");
    return false;
  }
  }
}

bool MemoryAccessPattern::isAccessPatternRelaxed(AccessPattern A,
                                                 AccessPattern B) {
  if (A == B) {
    return true;
  }
  switch (A) {
  case UNCONDITIONAL: {
    return true;
  }
  case CONDITIONAL_UPDATE_ONLY:
  case CONDITIONAL_ACCESS_ONLY: {
    return B == SAME_CONDITION;
  }
  case SAME_CONDITION: {
    return false;
  }
  default: {
    llvm_unreachable("Illegal memory access pattern.");
    return false;
  }
  }
}

void MemoryAccessPattern::addMissingAccess() {

  if (this->ComputingFSMs.empty()) {
    auto IndirectBase = MemoryAccessPattern::isIndirect(this->MemInst);
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(UNCONDITIONAL, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_ACCESS_ONLY, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_UPDATE_ONLY, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(SAME_CONDITION, IndirectBase));
  }

  // Add the first missing access.
  for (auto FSM : this->ComputingFSMs) {
    FSM->addMissingAccess();
  }
}

void MemoryAccessPattern::addAccess(uint64_t Addr) {

  if (this->ComputingFSMs.empty()) {
    auto IndirectBase = MemoryAccessPattern::isIndirect(this->MemInst);
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(UNCONDITIONAL, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_ACCESS_ONLY, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_UPDATE_ONLY, IndirectBase));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(SAME_CONDITION, IndirectBase));
  }

  for (auto FSM : this->ComputingFSMs) {
    FSM->addAccess(Addr);
  }
}

void MemoryAccessPattern::endStream() {
  assert(!this->ComputingFSMs.empty() && "Ending stream not computed.");

  AccessPatternFSM *NewFSM = nullptr;
  Pattern NewAddrPattern;
  for (auto FSM : this->ComputingFSMs) {
    if (FSM->getState() == AccessPatternFSM::StateT::SUCCESS) {

      if (this->MemInst->getName() == "tmp18") {
        DEBUG(llvm::errs() << "tmp18 has access pattern "
                           << formatAccessPattern(FSM->getAccessPattern())
                           << " with address pattern "
                           << formatPattern(
                                  FSM->getAddressPattern().CurrentPattern)
                           << '\n');
      }

      if (NewFSM == nullptr) {
        NewFSM = FSM;
        NewAddrPattern = FSM->getAddressPattern().CurrentPattern;
      } else {
        // Try to find the most constrainted FSM.
        auto AddrPattern = FSM->getAddressPattern().CurrentPattern;
        if (AddrPattern != NewAddrPattern &&
            MemoryAccessPattern::isAddressPatternRelaxed(AddrPattern,
                                                         NewAddrPattern)) {
          // NewFSM is more relaxted than FSM (not equal).
          NewFSM = FSM;
        }
      }
    }
  }

  if (this->MemInst->getName() == "tmp18") {
    DEBUG(llvm::errs() << "tmp18 picked access pattern "
                       << formatAccessPattern(NewFSM->getAccessPattern())
                       << " with address pattern "
                       << formatPattern(
                              NewFSM->getAddressPattern().CurrentPattern)
                       << '\n');
  }

  if (this->ComputedPatternPtr == nullptr) {
    this->ComputedPatternPtr = new ComputedPattern(*NewFSM);
  } else {
    this->ComputedPatternPtr->merge(*NewFSM);
  }

  for (auto &FSM : this->ComputingFSMs) {
    delete FSM;
    FSM = nullptr;
  }

  this->ComputingFSMs.clear();
}

const MemoryAccessPattern::ComputedPattern &
MemoryAccessPattern::getPattern() const {
  // We are not sure about the pattern.
  assert(this->ComputedPatternPtr != nullptr &&
         "Failed getting memory access pattern for static instruction.");

  return *this->ComputedPatternPtr;
}

bool MemoryAccessPattern::computed() const {
  return this->ComputedPatternPtr != nullptr;
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

std::string MemoryAccessPattern::formatAccessPattern(AccessPattern AccPattern) {
  switch (AccPattern) {
  case UNCONDITIONAL: {
    return "UNCOND";
  }
  case CONDITIONAL_ACCESS_ONLY: {
    return "COND_ACCESS";
  }
  case CONDITIONAL_UPDATE_ONLY: {
    return "COND_UPDATE";
  }
  case SAME_CONDITION: {
    return "SAME_COND";
  }
  default: {
    llvm_unreachable("Illegal access pattern to be converted to string.");
  }
  }
}

llvm::Instruction *MemoryAccessPattern::isIndirect(llvm::Instruction *Inst) {
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
    const AccessPatternFSM &NewFSM) {
  const auto &AddrPatternFSM = NewFSM.getAddressPattern();
  this->Accesses += NewFSM.Accesses;
  this->Updates += AddrPatternFSM.Updates;
  this->Iters += NewFSM.Iters;
  this->StreamCount++;
  if (MemoryAccessPattern::isAddressPatternRelaxed(
          this->CurrentPattern, AddrPatternFSM.CurrentPattern)) {
    this->CurrentPattern = AddrPatternFSM.CurrentPattern;
  }
  auto NewAccPattern = NewFSM.getAccessPattern();
  if (MemoryAccessPattern::isAccessPatternRelaxed(this->AccPattern,
                                                  NewAccPattern)) {
    this->AccPattern = NewAccPattern;
  }
}

void MemoryAccessPattern::finalizePattern() {
  if (this->ComputedPatternPtr == nullptr) {
    return;
  }
  if (static_cast<float>(this->ComputedPatternPtr->Accesses) /
          static_cast<float>(this->ComputedPatternPtr->StreamCount) <
      3.0) {
    // In such case we believe this is not a stream.
    this->ComputedPatternPtr->CurrentPattern = RANDOM;
  }
}

#undef DEBUG_TYPE