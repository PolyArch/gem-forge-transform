#ifndef LLVM_TDG_FUNCTIONAL_STREAM_H
#define LLVM_TDG_FUNCTIONAL_STREAM_H

#include "FunctionalStreamPattern.h"

#include "stream/MemStream.h"
#include "stream/Stream.h"

#include "ExecutionEngine/Interpreter/Interpreter.h"

#include <string>

class FunctionalStreamEngine;
class FunctionalStream {
public:
  FunctionalStream(Stream *_S, FunctionalStreamEngine *_SE);

  FunctionalStream(const FunctionalStream &Other) = delete;
  FunctionalStream(FunctionalStream &&Other) = delete;
  FunctionalStream &operator=(const FunctionalStream &Other) = delete;
  FunctionalStream &operator=(FunctionalStream &&Other) = delete;

  uint64_t getValue() const { return this->CurrentValue; }
  void configure(DataGraph *DG);
  void step(DataGraph *DG);

private:
  Stream *S;
  FunctionalStreamEngine *SE;

  std::unordered_map<const llvm::Value *, FunctionalStream *> BaseStreams;
  std::unordered_set<FunctionalStream *> DependentStreams;

  FunctionalStreamPattern Pattern;

  uint64_t CurrentValue;

  std::list<uint64_t> FIFO;

  uint64_t computeAddress(DataGraph *DG) const;
  void setGenericValueFromDynamicValue(llvm::Type *Type,
                                       llvm::GenericValue &GenericVal,
                                       const DynamicValue &DynamicVal) const;
  void setGenericValueFromUInt64(llvm::Type *Type,
                                 llvm::GenericValue &GenericVal,
                                 const uint64_t &Val) const;
  uint64_t extractAddressFromGenericValue(llvm::Type *Type,
                                          llvm::GenericValue &GenericVal) const;
};

#endif