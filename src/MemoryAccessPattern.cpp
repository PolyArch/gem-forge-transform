#include "MemoryAccessPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "MemoryAccessPattern"

MemoryAccessPattern::AddressPatternFSM::AddressPatternFSM(
    Pattern _CurrentPattern)
    : State(UNKNOWN), CurrentPattern(_CurrentPattern), Updates(0),
      PrevAddress(0), Base(0), StrideI(0), I(0), NI(0), StrideJ(0), J(0) {
  if (this->CurrentPattern == MemoryAccessPattern::Pattern::UNKNOWN) {
    // Success at the beginning for unknwon pattern.
    this->State = SUCCESS;
  }
  if (this->CurrentPattern == RANDOM) {
    // Always success for random pattern.
    this->State = SUCCESS;
  }
  if (this->CurrentPattern == QUARDRIC) {
    // Always fail for QUARDRIC pattern for now.
    this->State = FAILURE;
  }
}

void MemoryAccessPattern::AddressPatternFSM::update(uint64_t Addr) {

  if (this->State == FAILURE) {
    return;
  }

  this->PrevAddress = Addr;
  this->Updates++;
  switch (this->CurrentPattern) {
  case UNKNOWN: {
    // We have at least one confirmed address, not unknown.
    this->State = FAILURE;
    break;
  }
  case CONSTANT: {
    switch (this->State) {
    case UNKNOWN: {
      this->State = SUCCESS;
      this->Base = Addr;
      break;
    }
    case SUCCESS: {
      if (this->Base != Addr) {
        this->State = FAILURE;
      }
      break;
    }
    case FAILURE: {
      llvm_unreachable("FAILURE state in CONSTANT.");
    }
    default: { llvm_unreachable("Illegal state in CONSTANT."); }
    }
    break;
  }
  case LINEAR: {
    switch (this->State) {
    case UNKNOWN: {
      this->ConfirmedAddrs.emplace_back(this->Updates - 1, Addr);
      if (this->ConfirmedAddrs.size() == 2) {
        auto Addr0 = this->ConfirmedAddrs.front().second;
        auto Idx0 = this->ConfirmedAddrs.front().first;
        auto Addr1 = this->ConfirmedAddrs.back().second;
        auto Idx1 = this->ConfirmedAddrs.back().first;
        // No need to worry about overflow.
        auto AddrDiff = Addr1 - Addr0;
        auto IdxDiff = Idx1 - Idx0;
        if (AddrDiff % IdxDiff != 0) {
          this->State = FAILURE;
        } else {
          this->StrideI = AddrDiff / IdxDiff;
          this->Base = Addr0 - Idx0 * this->StrideI;
          this->I = Idx1;
          this->State = SUCCESS;
          DEBUG(llvm::errs() << "StrideI " << this->StrideI << " Base "
                             << this->Base << " I " << this->I << '\n');
        }
      }
      break;
    }
    case SUCCESS: {
      auto CurrentAddr = this->Base + this->StrideI * (this->I + 1);
      if (Addr == CurrentAddr) {
        // We are still accessing the same address.
        this->I++;
      } else {
        this->State = FAILURE;
      }
      break;
    }
    case FAILURE: {
      llvm_unreachable("FAILURE state in LINEAR.");
    }
    default: { llvm_unreachable("Illegal state in LINEAR."); }
    }
    break;
  }
  case QUARDRIC: {

    // switch (this->State) {
    // case UNKNOWN: {
    //   this->ConfirmedAddrs.emplace_back(this->Updates - 1, Addr);
    //   if (this->ConfirmedAddrs.size() == 3) {
    //     auto Addr0 = this->ConfirmedAddrs.front().second;
    //     auto Idx0 = this->ConfirmedAddrs.front().first;
    //     auto Addr1 = this->ConfirmedAddrs.back().second;
    //     auto Idx1 = this->ConfirmedAddrs.back().first;
    //     // No need to worry about overflow.
    //     auto AddrDiff = Addr1 - Addr0;
    //     auto IdxDiff = Idx1 - Idx0;
    //     if (AddrDiff % IdxDiff != 0) {
    //       this->State = FAILURE;
    //     } else {
    //       this->StrideI = AddrDiff / IdxDiff;
    //       this->Base = Addr0 - Idx0 * this->StrideI;
    //       this->I = Idx1;
    //       this->State = SUCCESS;
    //     }
    //   }
    //   break;
    // }
    // case SUCCESS: {
    //   auto CurrentAddr = this->Base + this->StrideI * (this->I + 1);
    //   if (Addr == CurrentAddr) {
    //     // We are still accessing the same address.
    //     this->I++;
    //   } else {
    //     this->State = FAILURE;
    //   }
    //   break;
    // }
    // case FAILURE: {
    //   llvm_unreachable("FAILURE state in QUARDRIC.");
    // }
    // default: { llvm_unreachable("Illegal state in QUARDRIC."); }
    // }

    // // Have not reached the limit.
    // auto CurrentAddr =
    //     this->Base + this->StrideJ * this->J + this->StrideI * (this->I + 1);
    // if (this->I + 1 == this->NI) {
    //   // Reached the limit.
    //   CurrentAddr = this->Base + this->StrideJ * (this->J + 1);
    // }

    // if (Addr == CurrentAddr) {
    //   // We are in the same pattern, update the index.
    //   this->I++;
    //   if (this->I == this->NI) {
    //     this->I = 0;
    //     this->J++;
    //   }
    // } else {
    //   // This is not what we can handle, switch to random.
    //   this->CurrentPattern = RANDOM;
    // }
    break;
  }
  case RANDOM: {
    // Nothing to do. We always succeed.
    break;
  }
  default: {
    llvm_unreachable("Illegal memory access pattern.");
    break;
  }
  }
}

void MemoryAccessPattern::AddressPatternFSM::updateMissing() {

  if (this->State == FAILURE) {
    return;
  }

  // Implicitly assume it follows the current pattern, and update the
  // statistics and induction variable.
  this->Updates++;
  switch (this->CurrentPattern) {
  case LINEAR: {
    if (this->State == SUCCESS) {
      this->PrevAddress = this->Base + this->StrideI * (this->I + 1);
      this->I++;
    }
    break;
  }
  case QUARDRIC: {
    if (this->State == SUCCESS) {
      if (this->I + 1 == this->NI) {
        // Reached the limit.
        this->PrevAddress = this->Base + this->StrideJ * (this->J + 1);
        this->I = 0;
        this->J++;
      } else {
        this->PrevAddress = this->Base + this->StrideJ * this->J +
                            this->StrideI * (this->I + 1);
        this->I++;
      }
    }

    break;
  }
  default: { break; }
  }
}

MemoryAccessPattern::AccessPatternFSM::AccessPatternFSM(
    AccessPattern _AccPattern)
    : Accesses(0), Iters(0), State(UNKNOWN), AccPattern(_AccPattern) {
  this->AddressPatterns.emplace_back(MemoryAccessPattern::Pattern::UNKNOWN);
  this->AddressPatterns.emplace_back(MemoryAccessPattern::Pattern::LINEAR);
  this->AddressPatterns.emplace_back(MemoryAccessPattern::Pattern::QUARDRIC);
  this->AddressPatterns.emplace_back(MemoryAccessPattern::Pattern::RANDOM);
}

const MemoryAccessPattern::AddressPatternFSM &
MemoryAccessPattern::AccessPatternFSM::getAddressPattern() const {
  for (const auto &AddrPattern : this->AddressPatterns) {
    if (AddrPattern.State != AddressPatternFSM::StateT::FAILURE) {
      // UNKNOWN is also treated as SUCCESS as we are optimistic.
      return AddrPattern;
    }
  }
  llvm_unreachable("Failed to get one single succeeded address pattern.");
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
    this->feedUpdateMissingToAddrPatterns();
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
    this->feedUpdateToAddrPatterns(Addr);
    break;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_ACCESS_ONLY: {
    this->feedUpdateToAddrPatterns(Addr);
    break;
  }
  case MemoryAccessPattern::AccessPattern::CONDITIONAL_UPDATE_ONLY: {
    for (auto &AddrPattern : this->AddressPatterns) {
      if (Addr != AddrPattern.PrevAddress) {
        if (AddrPattern.CurrentPattern == LINEAR) {
          DEBUG(llvm::errs() << "Update linear with " << Addr << " Prev addr "
                             << AddrPattern.PrevAddress << '\n');
        }
        AddrPattern.update(Addr);
      }
    }
    break;
  }
  case MemoryAccessPattern::AccessPattern::SAME_CONDITION: {
    this->feedUpdateToAddrPatterns(Addr);
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

  this->initialize();

  // Add the first missing access.
  for (auto FSM : this->ComputingFSMs) {
    FSM->addMissingAccess();
  }
}

void MemoryAccessPattern::addAccess(uint64_t Addr) {

  this->initialize();

  this->Footprint.access(Addr);

  if (this->MemInst->getName() == "tmp22") {
    DEBUG(llvm::errs() << "tmp18 accessed " << Addr << '\n');
  }

  for (auto FSM : this->ComputingFSMs) {
    if (FSM->getAccessPattern() == CONDITIONAL_UPDATE_ONLY) {
      DEBUG(llvm::errs() << "update conditional update only\n");
    }
    FSM->addAccess(Addr);
  }
}

void MemoryAccessPattern::endStream() {
  // this->initialize();
  if (this->ComputingFSMs.empty()) {
    // Somehow we failed to analyze the stream.
    return;
  }

  AccessPatternFSM *NewFSM = nullptr;
  Pattern NewAddrPattern;
  for (auto FSM : this->ComputingFSMs) {
    if (FSM->getState() == AccessPatternFSM::StateT::SUCCESS) {

      if (this->MemInst->getName() == "tmp22") {
        DEBUG(llvm::errs() << "tmp18 has access pattern "
                           << formatAccessPattern(FSM->getAccessPattern())
                           << " with address pattern "
                           << formatPattern(
                                  FSM->getAddressPattern().CurrentPattern)
                           << " with accesses " << FSM->Accesses
                           << " with updates "
                           << FSM->getAddressPattern().Updates << '\n');
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

  if (this->MemInst->getName() == "tmp22") {
    DEBUG(llvm::errs() << "tmp22 picked access pattern "
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
    // this->ComputedPatternPtr->CurrentPattern = RANDOM;
  }
}

void MemoryAccessPattern::initialize() {
  if (this->ComputingFSMs.empty()) {
    this->ComputingFSMs.push_back(new AccessPatternFSM(UNCONDITIONAL));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_ACCESS_ONLY));
    this->ComputingFSMs.push_back(
        new AccessPatternFSM(CONDITIONAL_UPDATE_ONLY));
    this->ComputingFSMs.push_back(new AccessPatternFSM(SAME_CONDITION));
  }
}

#undef DEBUG_TYPE