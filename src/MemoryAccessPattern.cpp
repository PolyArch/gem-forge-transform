#include "MemoryAccessPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "MemoryAccessPattern"

void MemoryAccessPattern::UnknownAddressPatternFSM::update(uint64_t Addr) {
  if (this->State == FAILURE) {
    return;
  }
  this->PrevAddress = Addr;
  this->Updates++;
  // We have at least one confirmed address, not unknown.
  this->State = FAILURE;
}

void MemoryAccessPattern::UnknownAddressPatternFSM::updateMissing() {
  // Do nothing for unknown pattern.
}

void MemoryAccessPattern::ConstAddressPatternFSM::update(uint64_t Addr) {
  if (this->State == FAILURE) {
    return;
  }
  this->Updates++;
  if (this->State == SUCCESS && this->PrevAddress != Addr) {
    this->State = FAILURE;
  } else if (this->State == UNKNOWN) {
    this->PrevAddress = Addr;
    this->State = SUCCESS;
  }
}

void MemoryAccessPattern::ConstAddressPatternFSM::updateMissing() {
  // Do nothing here for constant pattern.
}

void MemoryAccessPattern::LinearAddressPatternFSM::update(uint64_t Addr) {
  if (this->State == FAILURE) {
    return;
  }
  this->PrevAddress = Addr;
  this->Updates++;
  if (this->State == UNKNOWN) {
    this->ConfirmedAddrs.emplace_back(this->Updates - 1, Addr);
    if (this->ConfirmedAddrs.size() == 2) {
      auto BaseStridePair = AddressPatternFSM::computeLinearBaseStride(
          this->ConfirmedAddrs.front(), this->ConfirmedAddrs.back());
      this->Base = BaseStridePair.first;
      this->Stride = BaseStridePair.second;
      if (this->Base == 0 || this->Stride == 0) {
        this->State = FAILURE;
      } else {
        this->I = this->Updates - 1;
        this->State = SUCCESS;
        // DEBUG(llvm::errs() << "Stride " << this->Stride << " Base "
        //                    << this->Base << " I " << this->I << '\n');
      }
    }
  } else if (this->State == SUCCESS) {
    auto CurrentAddr = this->computeNextAddr();
    if (Addr == CurrentAddr) {
      // We are still accessing the same address.
      this->I++;
    } else {
      this->State = FAILURE;
    }
  }
}

void MemoryAccessPattern::LinearAddressPatternFSM::updateMissing() {
  if (this->State == FAILURE) {
    return;
  }
  // Implicitly assume it follows the current pattern, and update the
  // statistics and induction variable.
  this->Updates++;
  if (this->State == SUCCESS) {
    this->PrevAddress = this->computeNextAddr();
    this->I++;
  }
}

uint64_t MemoryAccessPattern::LinearAddressPatternFSM::computeNextAddr() const {
  return this->Base + this->Stride * (this->I + 1);
}

void MemoryAccessPattern::QuardricAddressPatternFSM::update(uint64_t Addr) {
  if (this->State == FAILURE) {
    return;
  }

  this->Updates++;
  this->PrevAddress = Addr;
  DEBUG(llvm::errs() << "get address " << Addr << " confirmed size "
                     << this->ConfirmedAddrs.size() << '\n');
  if (this->State == UNKNOWN) {
    this->ConfirmedAddrs.emplace_back(this->Updates - 1, Addr);
    if (this->ConfirmedAddrs.size() == 3) {
      if (this->isAligned()) {
        DEBUG(llvm::errs() << "Is aligned\n");
        // The three confirmed addresses is still aligned, remove the middle one
        // and keep waiting for future access.
        auto Iter = this->ConfirmedAddrs.begin();
        Iter++;
        this->ConfirmedAddrs.erase(Iter);
      } else {
        // This is not not aligned, we assume the third one is the first one in
        // the next row.
        auto SecondPairIter = this->ConfirmedAddrs.begin();
        SecondPairIter++;
        auto BaseStridePair = AddressPatternFSM::computeLinearBaseStride(
            this->ConfirmedAddrs.front(), *SecondPairIter);
        if (BaseStridePair.first == 0 || BaseStridePair.second == 0) {
          DEBUG(llvm::errs() << "Failed to compute the base stride for "
                                "quardric address pattern.\n");
          this->State = FAILURE;
        } else {
          this->Base = BaseStridePair.first;
          this->StrideI = BaseStridePair.second;
          this->I = 0;
          this->NI = this->Updates - 1;
          this->StrideJ = Addr - this->Base;
          this->J = 1;
          this->State = SUCCESS;
          DEBUG(llvm::errs()
                << "StrideJ " << this->StrideJ << " NI " << NI << '\n');
        }
      }
    }
  } else if (this->State == SUCCESS) {
    auto NextAddr = this->computeNextAddr();
    if (NextAddr != Addr) {
      this->State = FAILURE;
    } else {
      this->step();
    }
  }
}

void MemoryAccessPattern::QuardricAddressPatternFSM::updateMissing() {
  if (this->State == FAILURE) {
    return;
  }
  this->Updates++;
  if (this->State == SUCCESS) {
    this->PrevAddress = this->computeNextAddr();
    this->step();
  }
}

bool MemoryAccessPattern::QuardricAddressPatternFSM::isAligned() const {
  /**
   * Check if the three address is aligned.
   * i  j  k
   * vi vj vk
   * (vj - vi) * (j - i) = (vk - vi) * (k - i)
   */
  assert(this->ConfirmedAddrs.size() == 3 &&
         "There should be at least three addresses to check if aligned.");

  auto Iter = this->ConfirmedAddrs.begin();
  auto I = Iter->first;
  auto VI = Iter->second;
  Iter++;
  auto J = Iter->first;
  auto VJ = Iter->second;
  Iter++;
  auto K = Iter->first;
  auto VK = Iter->second;

  return (VJ - VI) * (K - I) == (VK - VI) * (J - I);
}

uint64_t
MemoryAccessPattern::QuardricAddressPatternFSM::computeNextAddr() const {
  if (this->I + 1 == this->NI) {
    return this->Base + (this->J + 1) * this->StrideJ;
  } else {
    return this->Base + this->J * this->StrideJ + (this->I + 1) * this->StrideI;
  }
}

void MemoryAccessPattern::QuardricAddressPatternFSM::step() {
  if (this->I + 1 == this->NI) {
    this->I = 0;
    this->J++;
  } else {
    this->I++;
  }
}

void MemoryAccessPattern::RandomAddressPatternFSM::update(uint64_t Addr) {
  this->Updates++;
  this->PrevAddress = Addr;
}

void MemoryAccessPattern::RandomAddressPatternFSM::updateMissing() {
  this->Updates++;
}

MemoryAccessPattern::AccessPatternFSM::AccessPatternFSM(
    AccessPattern _AccPattern)
    : Accesses(0), Iters(0), State(UNKNOWN), AccPattern(_AccPattern) {
  this->AddressPatterns.emplace_back(
      new MemoryAccessPattern::UnknownAddressPatternFSM());
  this->AddressPatterns.emplace_back(
      new MemoryAccessPattern::ConstAddressPatternFSM());
  this->AddressPatterns.emplace_back(
      new MemoryAccessPattern::LinearAddressPatternFSM());
  this->AddressPatterns.emplace_back(
      new MemoryAccessPattern::QuardricAddressPatternFSM());
  this->AddressPatterns.emplace_back(
      new MemoryAccessPattern::RandomAddressPatternFSM());
}

MemoryAccessPattern::AccessPatternFSM::~AccessPatternFSM() {
  for (auto &AddressFSM : this->AddressPatterns) {
    delete AddressFSM;
    AddressFSM = nullptr;
  }
  this->AddressPatterns.clear();
}

const MemoryAccessPattern::AddressPatternFSM &
MemoryAccessPattern::AccessPatternFSM::getAddressPattern() const {
  for (const auto &AddrPattern : this->AddressPatterns) {
    if (AddrPattern->State != AddressPatternFSM::StateT::FAILURE) {
      // UNKNOWN is also treated as SUCCESS as we are optimistic.
      return *AddrPattern;
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
      if (Addr != AddrPattern->PrevAddress) {
        AddrPattern->update(Addr);
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

  if (this->MemInst->getName() == "tmp14") {
    DEBUG(llvm::errs() << "tmp14 accessed " << Addr << '\n');
  }

  for (auto FSM : this->ComputingFSMs) {
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

      if (this->MemInst->getName() == "tmp14") {
        DEBUG(
            llvm::errs() << "tmp14 has access pattern "
                         << formatAccessPattern(FSM->getAccessPattern())
                         << " with address pattern "
                         << formatPattern(FSM->getAddressPattern().getPattern())
                         << " with accesses " << FSM->Accesses
                         << " with updates " << FSM->getAddressPattern().Updates
                         << '\n');
      }

      if (NewFSM == nullptr) {
        NewFSM = FSM;
        NewAddrPattern = FSM->getAddressPattern().getPattern();
      } else {
        // Try to find the most constrainted FSM.
        auto AddrPattern = FSM->getAddressPattern().getPattern();
        if (AddrPattern != NewAddrPattern &&
            MemoryAccessPattern::isAddressPatternRelaxed(AddrPattern,
                                                         NewAddrPattern)) {
          // NewFSM is more relaxted than FSM (not equal).
          NewFSM = FSM;
        }
      }
    }
  }

  if (this->MemInst->getName() == "tmp14") {
    DEBUG(
        llvm::errs() << "tmp14 picked access pattern "
                     << formatAccessPattern(NewFSM->getAccessPattern())
                     << " with address pattern "
                     << formatPattern(NewFSM->getAddressPattern().getPattern())
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
          this->CurrentPattern, AddrPatternFSM.getPattern())) {
    this->CurrentPattern = AddrPatternFSM.getPattern();
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