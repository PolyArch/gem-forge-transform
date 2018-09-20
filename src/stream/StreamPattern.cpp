#include "stream/StreamPattern.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "StreamPattern"

void StreamPattern::UnknownValuePatternFSM::update(uint64_t Val) {
  if (this->State == FAILURE) {
    return;
  }
  this->PrevValue = Val;
  this->Updates++;
  // We have at least one confirmed address, not unknown.
  this->State = FAILURE;
}

void StreamPattern::UnknownValuePatternFSM::updateMissing() {
  // Do nothing for unknown pattern.
}

void StreamPattern::ConstValuePatternFSM::update(uint64_t Val) {
  if (this->State == FAILURE) {
    return;
  }
  this->Updates++;
  if (this->State == SUCCESS && this->PrevValue != Val) {
    this->State = FAILURE;
  } else if (this->State == UNKNOWN) {
    this->PrevValue = Val;
    this->State = SUCCESS;
  }
}

void StreamPattern::ConstValuePatternFSM::updateMissing() {
  // Do nothing here for constant pattern.
}

void StreamPattern::ConstValuePatternFSM::fillInComputedPattern(
    ComputedPattern &OutPattern) const {
  OutPattern.ValPattern = ValuePattern::CONSTANT;
  OutPattern.Base = this->PrevValue;
}

void StreamPattern::LinearValuePatternFSM::update(uint64_t Val) {
  if (this->State == FAILURE) {
    return;
  }
  this->PrevValue = Val;
  this->Updates++;
  if (this->State == UNKNOWN) {
    this->ConfirmedVals.emplace_back(this->Updates - 1, Val);
    if (this->ConfirmedVals.size() == 2) {
      auto BaseStrideResult = ValuePatternFSM::computeLinearBaseStride(
          this->ConfirmedVals.front(), this->ConfirmedVals.back());
      if (BaseStrideResult.first == false) {
        this->State = FAILURE;
      } else {
        this->Base = BaseStrideResult.second.first;
        this->Stride = BaseStrideResult.second.second;
        this->I = this->Updates - 1;
        this->State = SUCCESS;
        // DEBUG(llvm::errs() << "Stride " << this->Stride << " Base "
        //                    << this->Base << " I " << this->I << '\n');
      }
    }
  } else if (this->State == SUCCESS) {
    auto CurrentVal = this->computeNextVal();
    if (Val == CurrentVal) {
      // We are still accessing the same address.
      this->I++;
    } else {
      this->State = FAILURE;
    }
  }
}

void StreamPattern::LinearValuePatternFSM::updateMissing() {
  if (this->State == FAILURE) {
    return;
  }
  // Implicitly assume it follows the current pattern, and update the
  // statistics and induction variable.
  this->Updates++;
  if (this->State == SUCCESS) {
    this->PrevValue = this->computeNextVal();
    this->I++;
  }
}

void StreamPattern::LinearValuePatternFSM::fillInComputedPattern(
    ComputedPattern &OutPattern) const {
  OutPattern.ValPattern = ValuePattern::LINEAR;
  OutPattern.Base = this->Base;
  OutPattern.StrideI = this->Stride;
}

uint64_t StreamPattern::LinearValuePatternFSM::computeNextVal() const {
  return this->Base + this->Stride * (this->I + 1);
}

void StreamPattern::QuardricValuePatternFSM::update(uint64_t Val) {
  if (this->State == FAILURE) {
    return;
  }

  this->Updates++;
  this->PrevValue = Val;
  DEBUG(llvm::errs() << "get address " << Val << " confirmed size "
                     << this->ConfirmedVals.size() << '\n');
  if (this->State == UNKNOWN) {
    this->ConfirmedVals.emplace_back(this->Updates - 1, Val);
    if (this->ConfirmedVals.size() == 3) {
      if (this->isAligned()) {
        DEBUG(llvm::errs() << "Is aligned\n");
        // The three confirmed addresses is still aligned, remove the middle one
        // and keep waiting for future access.
        auto Iter = this->ConfirmedVals.begin();
        Iter++;
        this->ConfirmedVals.erase(Iter);
      } else {
        // This is not not aligned, we assume the third one is the first one in
        // the next row.
        auto SecondPairIter = this->ConfirmedVals.begin();
        SecondPairIter++;
        auto BaseStrideResult = ValuePatternFSM::computeLinearBaseStride(
            this->ConfirmedVals.front(), *SecondPairIter);
        if (!BaseStrideResult.first) {
          DEBUG(llvm::errs() << "Failed to compute the base stride for "
                                "quardric address pattern.\n");
          DEBUG(llvm::errs() << this->ConfirmedVals.front().first << " "
                             << this->ConfirmedVals.front().second << '\n');
          DEBUG(llvm::errs() << SecondPairIter->first << " "
                             << SecondPairIter->second << '\n');
          this->State = FAILURE;
        } else {
          this->Base = BaseStrideResult.second.first;
          this->StrideI = BaseStrideResult.second.second;
          this->I = 0;
          this->NI = this->Updates - 1;
          this->StrideJ = Val - this->Base;
          this->J = 1;
          this->State = SUCCESS;
          DEBUG(llvm::errs()
                << "StrideJ " << this->StrideJ << " NI " << NI << '\n');
        }
      }
    }
  } else if (this->State == SUCCESS) {
    auto NextVal = this->computeNextVal();
    if (NextVal != Val) {
      this->State = FAILURE;
    } else {
      this->step();
    }
  }
}

void StreamPattern::QuardricValuePatternFSM::updateMissing() {
  if (this->State == FAILURE) {
    return;
  }
  this->Updates++;
  if (this->State == SUCCESS) {
    this->PrevValue = this->computeNextVal();
    this->step();
  }
}

void StreamPattern::QuardricValuePatternFSM::fillInComputedPattern(
    ComputedPattern &OutPattern) const {
  OutPattern.ValPattern = ValuePattern::QUARDRIC;
  OutPattern.Base = this->Base;
  OutPattern.StrideI = this->StrideI;
  OutPattern.NI = this->NI;
  OutPattern.StrideJ = this->StrideJ;
}

bool StreamPattern::QuardricValuePatternFSM::isAligned() const {
  /**
   * Check if the three address is aligned.
   * i  j  k
   * vi vj vk
   * (vj - vi) * (j - i) = (vk - vi) * (k - i)
   */
  assert(this->ConfirmedVals.size() == 3 &&
         "There should be at least three addresses to check if aligned.");

  auto Iter = this->ConfirmedVals.begin();
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

uint64_t StreamPattern::QuardricValuePatternFSM::computeNextVal() const {
  if (this->I + 1 == this->NI) {
    return this->Base + (this->J + 1) * this->StrideJ;
  } else {
    return this->Base + this->J * this->StrideJ + (this->I + 1) * this->StrideI;
  }
}

void StreamPattern::QuardricValuePatternFSM::step() {
  if (this->I + 1 == this->NI) {
    this->I = 0;
    this->J++;
  } else {
    this->I++;
  }
}

void StreamPattern::RandomValuePatternFSM::update(uint64_t Val) {
  this->Updates++;
  this->PrevValue = Val;
}

void StreamPattern::RandomValuePatternFSM::updateMissing() { this->Updates++; }

StreamPattern::AccessPatternFSM::AccessPatternFSM(AccessPattern _AccPattern)
    : Accesses(0), Iters(0), State(UNKNOWN), AccPattern(_AccPattern) {
  this->ValuePatterns.emplace_back(new StreamPattern::UnknownValuePatternFSM());
  this->ValuePatterns.emplace_back(new StreamPattern::ConstValuePatternFSM());
  this->ValuePatterns.emplace_back(new StreamPattern::LinearValuePatternFSM());
  this->ValuePatterns.emplace_back(
      new StreamPattern::QuardricValuePatternFSM());
  this->ValuePatterns.emplace_back(new StreamPattern::RandomValuePatternFSM());
}

StreamPattern::AccessPatternFSM::~AccessPatternFSM() {
  for (auto &ValueFSM : this->ValuePatterns) {
    delete ValueFSM;
    ValueFSM = nullptr;
  }
  this->ValuePatterns.clear();
}

const StreamPattern::ValuePatternFSM &
StreamPattern::AccessPatternFSM::getValuePatternFSM() const {
  for (const auto &ValPattern : this->ValuePatterns) {
    if (ValPattern->State != ValuePatternFSM::StateT::FAILURE) {
      // UNKNOWN is also treated as SUCCESS as we are optimistic.
      return *ValPattern;
    }
  }
  llvm_unreachable("Failed to get one single succeeded address pattern.");
}

void StreamPattern::AccessPatternFSM::addMissingAccess() {
  this->Iters++;
  if (this->State == FAILURE) {
    return;
  }
  switch (this->AccPattern) {
  case StreamPattern::AccessPattern::UNCONDITIONAL: {
    this->State = FAILURE;
    return;
  }
  case StreamPattern::AccessPattern::CONDITIONAL_ACCESS_ONLY: {
    this->feedUpdateMissingToValPatterns();
    break;
  }
  case StreamPattern::AccessPattern::CONDITIONAL_UPDATE_ONLY: {
    this->State = FAILURE;
    return;
  }
  case StreamPattern::AccessPattern::SAME_CONDITION: {
    break;
  }
  default: { llvm_unreachable("Illegal access pattern."); }
  }
  this->State = SUCCESS;
}

void StreamPattern::AccessPatternFSM::addAccess(uint64_t Val) {
  this->Iters++;
  this->Accesses++;
  if (this->State == FAILURE) {
    return;
  }
  switch (this->AccPattern) {
  case StreamPattern::AccessPattern::UNCONDITIONAL: {
    this->feedUpdateToValPatterns(Val);
    break;
  }
  case StreamPattern::AccessPattern::CONDITIONAL_ACCESS_ONLY: {
    this->feedUpdateToValPatterns(Val);
    break;
  }
  case StreamPattern::AccessPattern::CONDITIONAL_UPDATE_ONLY: {
    for (auto &ValPattern : this->ValuePatterns) {
      if (Val != ValPattern->PrevValue) {
        ValPattern->update(Val);
      }
    }
    break;
  }
  case StreamPattern::AccessPattern::SAME_CONDITION: {
    this->feedUpdateToValPatterns(Val);
    break;
  }
  }
  this->State = SUCCESS;
}

bool StreamPattern::isValuePatternRelaxed(ValuePattern A, ValuePattern B) {
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

bool StreamPattern::isAccessPatternRelaxed(AccessPattern A, AccessPattern B) {
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

void StreamPattern::addMissingAccess() {

  this->initialize();
  this->History.emplace_back(false, 0);

  // Add the first missing access.
  for (auto FSM : this->ComputingFSMs) {
    FSM->addMissingAccess();
  }
}

void StreamPattern::addAccess(uint64_t Val) {

  this->initialize();
  this->History.emplace_back(true, Val);

  for (auto FSM : this->ComputingFSMs) {
    FSM->addAccess(Val);
  }
}

const StreamPattern::ComputedPattern StreamPattern::endStream() {
  ComputedPattern ReturnPattern(this->History);
  this->History.clear();
  if (this->ComputingFSMs.empty()) {
    // Somehow we failed to analyze the stream.
    return ReturnPattern;
  }

  AccessPatternFSM *NewAccFSM = nullptr;
  const ValuePatternFSM *NewValFSM = nullptr;
  ValuePattern NewValPattern;
  for (auto AccFSM : this->ComputingFSMs) {
    if (AccFSM->getState() == AccessPatternFSM::StateT::SUCCESS) {

      if (NewAccFSM == nullptr) {
        NewAccFSM = AccFSM;
        NewValFSM = &AccFSM->getValuePatternFSM();
        NewValPattern = NewValFSM->getValuePattern();
      } else {
        // Try to find the most constrainted FSM.
        auto ValFSM = &AccFSM->getValuePatternFSM();
        auto ValPattern = ValFSM->getValuePattern();
        if (ValPattern != NewValPattern &&
            StreamPattern::isValuePatternRelaxed(ValPattern, NewValPattern)) {
          // NewFSM is able to generated a more constraint (not equal) value
          // pattern.
          NewAccFSM = AccFSM;
          NewValFSM = ValFSM;
          NewValPattern = ValPattern;
        }
      }
    }
  }
  ReturnPattern.AccPattern = NewAccFSM->getAccessPattern();
  ReturnPattern.Accesses = NewAccFSM->Accesses;
  ReturnPattern.Iters = NewAccFSM->Iters;
  ReturnPattern.Updates = NewValFSM->Updates;
  NewValFSM->fillInComputedPattern(ReturnPattern);

  if (this->AggregatedPatternPtr == nullptr) {
    this->AggregatedPatternPtr = new AggregatedPattern(*NewAccFSM);
  } else {
    this->AggregatedPatternPtr->merge(*NewAccFSM);
  }

  for (auto &FSM : this->ComputingFSMs) {
    delete FSM;
    FSM = nullptr;
  }

  this->ComputingFSMs.clear();
  return ReturnPattern;
}

const StreamPattern::AggregatedPattern &StreamPattern::getPattern() const {
  // We are not sure about the pattern.
  assert(this->AggregatedPatternPtr != nullptr &&
         "Failed getting memory access pattern for static instruction.");

  return *this->AggregatedPatternPtr;
}

bool StreamPattern::computed() const {
  return this->AggregatedPatternPtr != nullptr;
}

std::string StreamPattern::formatValuePattern(ValuePattern ValPattern) {
  switch (ValPattern) {
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

std::string StreamPattern::formatAccessPattern(AccessPattern AccPattern) {
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

void StreamPattern::AggregatedPattern::merge(const AccessPatternFSM &NewFSM) {
  const auto &ValPatternFSM = NewFSM.getValuePatternFSM();
  this->Accesses += NewFSM.Accesses;
  this->Updates += ValPatternFSM.Updates;
  this->Iters += NewFSM.Iters;
  this->StreamCount++;
  if (StreamPattern::isValuePatternRelaxed(this->ValPattern,
                                           ValPatternFSM.getValuePattern())) {
    this->ValPattern = ValPatternFSM.getValuePattern();
  }
  auto NewAccPattern = NewFSM.getAccessPattern();
  if (StreamPattern::isAccessPatternRelaxed(this->AccPattern, NewAccPattern)) {
    this->AccPattern = NewAccPattern;
  }
}

void StreamPattern::finalizePattern() {
  if (this->AggregatedPatternPtr == nullptr) {
    return;
  }
  if (static_cast<float>(this->AggregatedPatternPtr->Accesses) /
          static_cast<float>(this->AggregatedPatternPtr->StreamCount) <
      3.0) {
    // In such case we believe this is not a stream.
    // this->AggregatedPatternPtr->CurrentPattern = RANDOM;
  }
}

void StreamPattern::initialize() {
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