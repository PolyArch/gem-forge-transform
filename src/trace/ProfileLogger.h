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
  ProfileLogger();

  // Increase the counter of the specific basic block.
  void addBasicBlock(const std::string &Func, const std::string &BB);

  // Serialize the profile result to a file.
  void serializeToFile(const std::string &FileName);

  //   // Begin an interval.
  //   void beginInterval(uint64_t DynamicInstSeqNum);

  //   // End an interval.
  //   void endInterval(uint64_t DynamicInstSeqNum);

 private:
  using FunctionProfileMapT =
      std::unordered_map<std::string,
                         std::unordered_map<std::string, uint64_t>>;
  using ProtobufFunctionProfileMapT =
      ::google::protobuf::Map<::std::string, ::LLVM::TDG::FunctionProfile>;
  FunctionProfileMapT Profile;
  std::unordered_map<std::string, uint64_t> FuncProfile;

  FunctionProfileMapT IntervalProfile;
  uint64_t IntervalLHS;

  uint64_t BBCount;

  LLVM::TDG::Profile ProtobufProfile;

  /**
   * Convert a FunctionProfileMapT to Protobuf message FunctionProfile.
   */
  void convertFunctionProfileMapTToProtobufFunctionProfile(
      const FunctionProfileMapT &In, ProtobufFunctionProfileMapT &Out) const;
};
#endif