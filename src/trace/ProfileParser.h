#ifndef LLVM_TDG_PROFILE_PARSER_H
#define LLVM_TDG_PROFILE_PARSER_H

#include "ProfileMessage.pb.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

class ProfileParser {
public:
  // Parse from a file.
  ProfileParser();

  // Get the counter of the specific basic block.
  uint64_t countBasicBlock(llvm::BasicBlock *BB) const;

private:
  LLVM::TDG::Profile Profile;
};

#endif