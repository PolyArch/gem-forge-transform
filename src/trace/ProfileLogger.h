#ifndef LLVM_TDG_PROFILE_LOGGER_H
#define LLVM_TDG_PROFILE_LOGGER_H

#include <string>

// A thin wrapper over the LLVM::TDG::Profile structure.
// This can only be used for single thread.
class ProfileLogger {
public:
  // Increase the counter of the specific basic block.
  static void addBasicBlock(const std::string &Func, const std::string &BB);

  // Serialize the profile result to a file.
  static void serializeToFile(const std::string &FileName);
};
#endif