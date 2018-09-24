#include "FunctionalStreamPattern.h"

#include "llvm/Support/ErrorHandling.h"

FunctionalStreamPattern::FunctionalStreamPattern(
    const std::string &_PatternPath)
    : PatternPath(_PatternPath), PatternStream(_PatternPath) {}

void FunctionalStreamPattern::configure() {
  assert(this->PatternStream.read(this->Pattern) &&
         "Failed to read in the next pattern.");

  this->idxI = 0;
  this->idxJ = 0;
}

std::pair<bool, uint64_t> FunctionalStreamPattern::getNextValue() {
  if (this->Pattern.val_pattern() == "CONSTANT") {
    return std::make_pair(true, this->Pattern.base());
  }

  if (this->Pattern.val_pattern() == "UNKNOWN") {
    // Jesus.
    return std::make_pair(false, static_cast<uint64_t>(0));
  }

  if (this->Pattern.val_pattern() == "LINEAR") {
    uint64_t nextValue =
        this->Pattern.base() + this->Pattern.stride_i() * this->idxI;
    this->idxI++;
    return std::make_pair(true, nextValue);
  }

  if (this->Pattern.val_pattern() == "QUARDRIC") {
    uint64_t nextValue = this->Pattern.base() +
                         this->Pattern.stride_i() * this->idxI +
                         this->Pattern.stride_j() * this->idxJ;
    this->idxI++;
    if (this->idxI == this->Pattern.ni()) {
      this->idxI = 0;
      this->idxJ++;
    }
    return std::make_pair(true, nextValue);
  }

  if (this->Pattern.val_pattern() == "RANDOM") {
    // Jesus.
    return std::make_pair(false, static_cast<uint64_t>(0));
  }

  llvm_unreachable("Unknown value pattern.");
}