#ifndef LLVM_TDG_FUNCTIONAL_STREAM_PATTERN_H
#define LLVM_TDG_FUNCTIONAL_STREAM_PATTERN_H

#include "stream/StreamMessage.pb.h"

#include <list>
#include <string>

class FunctionalStreamPattern {
public:
  using PatternListT = std::list<LLVM::TDG::StreamPattern>;

  FunctionalStreamPattern(const PatternListT &_ProtobufPatterns);

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
  std::pair<bool, uint64_t> getValue() const;

  void step();

private:
  const PatternListT &ProtobufPatterns;
  PatternListT::const_iterator NextPattern;
  PatternListT::const_iterator CurrentPattern;

  /**
   * Used for the linear and quardric pattern.
   */
  uint64_t idxI;
  uint64_t idxJ;
};

#endif