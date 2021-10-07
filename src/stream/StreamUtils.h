
#ifndef LLVM_TDG_STREAM_UTILS_H
#define LLVM_TDG_STREAM_UTILS_H

#include "UserSpecifiedInstructionSet.h"

class StreamUtils {
public:
  // Singleton StreamWhitelist.
  static UserSpecifiedInstructionSet &getStreamWhitelist() {
    return StreamWhitelist;
  }

private:
  static UserSpecifiedInstructionSet StreamWhitelist;
};

#endif