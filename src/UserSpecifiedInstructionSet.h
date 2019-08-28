#ifndef LLVM_TDG_USER_SPECIFIED_INSTRUCTION_LIST_H
#define LLVM_TDG_USER_SPECIFIED_INSTRUCTION_LIST_H

#include "Utils.h"

#include "llvm/IR/Instruction.h"

#include <unordered_set>

/**
 * Represent a user specified instruction list.
 * Basically parsed from a protobuf file.
 */

class UserSpecifiedInstructionSet {
public:
  UserSpecifiedInstructionSet() : Initialized(false) {}

  void parseFrom(const std::string &FileName);

  bool isInitialized() const { return this->Initialized; }

  bool contains(const llvm::Instruction *Inst) const;

private:
  bool Initialized;
  std::unordered_set<std::string> SpecifiedInstructionName;
  mutable std::unordered_set<const llvm::Instruction *>
      CheckedPositiveInstructions;
};

#endif