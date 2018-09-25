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

  uint64_t getValue() const { return this->CurrentValue; }
  void configure(DataGraph *DG);
  void step(DataGraph *DG);

  /**
   * When a load comes in, we update the value only if there is dependent stream
   * and the type is supported.
   */
  void updateLoadedValue(DataGraph *DG, const DynamicValue &DynamicVal);

  /**
   * Dump the history into the file when the stream ends.
   */
  void endStream();

private:
  Stream *S;
  FunctionalStreamEngine *SE;

  std::unordered_map<const llvm::Value *, FunctionalStream *> BaseStreams;
  std::unordered_set<FunctionalStream *> DependentStreams;
  std::unordered_set<FunctionalStream *> DependentStepStreams;

  FunctionalStreamPattern Pattern;

  LLVM::TDG::StreamHistory ProtobufHistoryEntry;
  Gem5ProtobufSerializer HistorySerializer;
  std::ofstream HistoryTextFStream;

  /**
   * For IVStream these two fields should always be the same.
   */
  uint64_t CurrentIdx; // 0 is reseved for the first empty state.
  static constexpr uint64_t InvalidIdx = 0;
  uint64_t CurrentAddress;
  uint64_t CurrentValue;
  bool IsAddressValid;
  bool IsValueValid;

  std::list<uint64_t> FIFO;

  void DEBUG_DUMP(llvm::raw_ostream &OS) const;

  std::pair<bool, uint64_t> computeAddress(DataGraph *DG) const;
  void setGenericValueFromDynamicValue(llvm::Type *Type,
                                       llvm::GenericValue &GenericVal,
                                       const DynamicValue &DynamicVal) const;
  void setGenericValueFromUInt64(llvm::Type *Type,
                                 llvm::GenericValue &GenericVal,
                                 const uint64_t &Val) const;
  uint64_t extractAddressFromGenericValue(llvm::Type *Type,
                                          llvm::GenericValue &GenericVal) const;

  void update(DataGraph *DG);

  /**
   * This will update myself and recursive update all the dependent streams.
   */
  void updateRecursively(DataGraph *DG);
};

#endif