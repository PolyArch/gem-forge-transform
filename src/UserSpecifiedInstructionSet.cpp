#include "UserSpecifiedInstructionSet.h"

#include <fstream>

void UserSpecifiedInstructionSet::parseFrom(const std::string &FileName) {
  assert(!this->isInitialized() &&
         "UserSpecifiedInstructionSet has already been initialized.");

  std::ifstream I(FileName);

  std::string InstName;
  while (std::getline(I, InstName)) {
    this->SpecifiedInstructionName.insert(InstName);
    llvm::errs() << InstName << '\n';
  }

  I.close();
  this->Initialized = true;
}

bool UserSpecifiedInstructionSet::contains(
    const llvm::Instruction *Inst) const {
  assert(this->isInitialized() &&
         "UserSpecifiedInstruction is not initialized.");
  if (this->CheckedPositiveInstructions.count(Inst) > 0) {
    // Cached fast path.
    return true;
  }
  auto InstName = Utils::formatLLVMInst(Inst);
  if (this->SpecifiedInstructionName.count(InstName) > 0) {
    this->CheckedPositiveInstructions.insert(Inst);
    return true;
  }
  return false;
}
