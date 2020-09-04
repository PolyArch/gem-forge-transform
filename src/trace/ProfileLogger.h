#ifndef LLVM_TDG_PROFILE_LOGGER_H
#define LLVM_TDG_PROFILE_LOGGER_H

#include "ProfileMessage.pb.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// A thin wrapper over the LLVM::TDG::Profile structure.
// This can only be used for single thread.
class ProfileLogger {
public:
  // Increase the counter of the specific basic block.
  void addInst(const std::string &Func, const std::string &BB);

  // Serialize the profile result to a file.
  void serializeToFile(const std::string &FileName);

  uint64_t getCurrentInstCount() const { return this->InstCount; }
  uint64_t getCurrentIntervalLHS() const { return this->IntervalLHS; }

  void saveAndRestartInterval();
  void discardAndRestartInterval();

private:
  using FunctionProfileMapT =
      std::unordered_map<std::string,
                         std::unordered_map<std::string, uint64_t>>;
  using ProtobufFunctionProfileMapT =
      ::google::protobuf::Map<::std::string, ::LLVM::TDG::FunctionProfile>;
  FunctionProfileMapT Profile;
  std::unordered_map<std::string, uint64_t> FuncProfile;

  FunctionProfileMapT IntervalProfile;
  uint64_t IntervalLHS = 0;

  uint64_t InstCount = 0;

  LLVM::TDG::Profile ProtobufProfile;

  /**
   * Convert a FunctionProfileMapT to Protobuf message FunctionProfile.
   */
  void convertFunctionProfileMapTToProtobufFunctionProfile(
      const FunctionProfileMapT &In, ProtobufFunctionProfileMapT &Out) const;
};

class FixedSizeIntervalProfileLogger {
public:
  void initialize(uint64_t _INTERVAL_SIZE);

  // Increase the counter of the specific basic block.
  void addInst(const std::string &Func, const std::string &BB);

  // Serialize the profile result to a file.
  void serializeToFile(const std::string &FileName) {
    this->Logger.serializeToFile(FileName);
  }

private:
  bool initialized = false;
  uint64_t INTERVAL_SIZE = 0;

  ProfileLogger Logger;
};
#endif