#ifndef LLVM_TDG_PROFILE_LOGGER_H
#define LLVM_TDG_PROFILE_LOGGER_H

#include <cstdint>
#include <string>
#include <unordered_map>

// A thin wrapper over the LLVM::TDG::Profile structure.
// This can only be used for single thread.
class ProfileLogger {
 public:
  ProfileLogger();

  // Increase the counter of the specific basic block.
  void addBasicBlock(const std::string &Func, const std::string &BB);

  // Serialize the profile result to a file.
  void serializeToFile(const std::string &FileName);

 private:
  std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>
      Profile;
  std::unordered_map<std::string, uint64_t> FuncProfile;
  uint64_t BBCount;
};
#endif