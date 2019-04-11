#ifndef LLVM_TDG_FUNCTIONAL_STREAM_H
#define LLVM_TDG_FUNCTIONAL_STREAM_H

#include "FunctionalStreamPattern.h"

#include "stream/MemStream.h"
#include "stream/Stream.h"
#include "stream/StreamMessage.pb.h"

#include "Gem5ProtobufSerializer.h"

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

  Stream *getStream() { return this->S; }
  const Stream *getStream() const { return this->S; }

  void addBaseStream(FunctionalStream *BaseStream);

  const std::unordered_set<FunctionalStream *> &getDependentFunctionalStreams()
      const {
    return this->DependentStreams;
  }

  uint64_t getAddress() const { return this->CurrentAddress; }
  uint64_t getValue() const { return this->CurrentValue; }
  void configure(DataGraph *DG);
  void step(DataGraph *DG);
  /**
   * Inform the functional stream that the current entry is actually used.
   */
  void access();

  void setValue(const DynamicValue &DynamicVal);
  void update(DataGraph *DG);

  void endStream();

  /**
   * Dump the history into the file when the stream ends.
   */
  void endAll();

 private:
  Stream *S;
  FunctionalStreamEngine *SE;
  MemoryFootprint MemFootprint;

  std::unordered_map<const llvm::Value *, FunctionalStream *> BaseStreams;
  std::unordered_set<FunctionalStream *> DependentStreams;

  FunctionalStreamPattern Pattern;

  std::list<LLVM::TDG::StreamHistory> ProtobufHistoryEntries;
  LLVM::TDG::StreamHistory *CurrentProtobufHistory;

  /**
   * For IVStream these two fields should always be the same.
   */
  uint64_t CurrentIdx;  // 0 is reseved for the first empty state.
  static constexpr uint64_t InvalidIdx = 0;
  uint64_t CurrentAddress;
  uint64_t CurrentValue;
  bool IsAddressValid;
  bool IsValueValid;
  bool CurrentEntryUsed;

  std::list<uint64_t> FIFO;

  void DEBUG_DUMP(llvm::raw_ostream &OS) const;

  void registerToStepRoot(FunctionalStream *NewStepDependentStream);

  std::pair<bool, uint64_t> computeAddress(DataGraph *DG) const;
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