#ifndef LLVM_TDG_FUNCTIONAL_STREAM_PATTERN_H
#define LLVM_TDG_FUNCTIONAL_STREAM_PATTERN_H

#include "Gem5ProtobufSerializer.h"
#include "stream/StreamMessage.pb.h"

#include <string>

class FunctionalStreamPattern {
public:
  FunctionalStreamPattern(const std::string &_PatternPath);

  /**
   * Read the next pattern from the stream.
   */
  void configure();

  /**
   * Return the next value of the pattern.
   * The first boolean indicating the value is valid.
   * Since we do not actually introduce the ability of compute indirect address,
   * for random pattern we do not know the missing address if it does not show
   * up in the trace.
   */
  std::pair<bool, uint64_t> getNextValue();

private:
  std::string PatternPath;
  Gem5ProtobufReader PatternStream;
  LLVM::TDG::StreamPattern Pattern;

  /**
   * Used for the linear and quardric pattern.
   */
  uint64_t idxI;
  uint64_t idxJ;

};

#endif