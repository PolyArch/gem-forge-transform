#include "FunctionalStreamPattern.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

FunctionalStreamPattern::FunctionalStreamPattern(
    const PatternListT &_ProtobufPatterns)
    : ProtobufPatterns(_ProtobufPatterns),
      NextPattern(_ProtobufPatterns.cbegin()) {}

void FunctionalStreamPattern::configure() {

  if (this->NextPattern == this->ProtobufPatterns.cend()) {
    llvm::errs() << "Failed to read the next pattern.";
  }

  this->CurrentPattern = this->NextPattern;
  ++this->NextPattern;

  this->idxI = 0;
  this->idxJ = 0;
}

std::pair<bool, uint64_t> FunctionalStreamPattern::getValue() const {
  if (this->CurrentPattern->val_pattern() == "CONSTANT") {
    return std::make_pair(true, this->CurrentPattern->base());
  }

  if (this->CurrentPattern->val_pattern() == "UNKNOWN") {
    // Jesus.
    return std::make_pair(false, static_cast<uint64_t>(0));
  }

  if (this->CurrentPattern->val_pattern() == "LINEAR") {
    uint64_t nextValue = this->CurrentPattern->base() +
                         this->CurrentPattern->stride_i() * this->idxI;
    return std::make_pair(true, nextValue);
  }

  if (this->CurrentPattern->val_pattern() == "QUARDRIC") {
    uint64_t nextValue = this->CurrentPattern->base() +
                         this->CurrentPattern->stride_i() * this->idxI +
                         this->CurrentPattern->stride_j() * this->idxJ;
    return std::make_pair(true, nextValue);
  }

  if (this->CurrentPattern->val_pattern() == "RANDOM") {
    // Jesus.
    return std::make_pair(false, static_cast<uint64_t>(0));
  }

  llvm_unreachable("Unknown value pattern.");
}

void FunctionalStreamPattern::step() {

  if (this->CurrentPattern->val_pattern() == "CONSTANT") {
    return;
  }

  if (this->CurrentPattern->val_pattern() == "UNKNOWN") {
    // Jesus.
    return;
  }

  if (this->CurrentPattern->val_pattern() == "LINEAR") {
    this->idxI++;
    return;
  }

  if (this->CurrentPattern->val_pattern() == "QUARDRIC") {
    this->idxI++;
    if (this->idxI == this->CurrentPattern->ni()) {
      this->idxI = 0;
      this->idxJ++;
    }
    return;
  }

  if (this->CurrentPattern->val_pattern() == "RANDOM") {
    // Jesus.
    return;
  }

  llvm_unreachable("Unknown value pattern.");
}