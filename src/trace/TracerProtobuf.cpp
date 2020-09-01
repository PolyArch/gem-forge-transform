// Implement the tracer interface with protobuf.
#include "../ProtobufSerializer.h"
#include "TraceMessage.pb.h"
#include "TracerImpl.h"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

void cleanup();

// Extension for multi-thread.
namespace {
struct ThreadState {
  GzipMultipleProtobufSerializer *serializer = nullptr;
  LLVM::TDG::DynamicLLVMTraceEntry protobufTraceEntry;
};

// Accessing this array does not require a lock.
static std::array<ThreadState, MaxNThreads> states;

} // namespace

inline GzipMultipleProtobufSerializer &getTraceFile(int tid) {
  auto &ts = states.at(tid);
  if (!ts.serializer) {
    ts.serializer =
        new GzipMultipleProtobufSerializer(getNewTraceFileName(tid));
    std::atexit(cleanup);
  }
  return *ts.serializer;
}

static uint64_t count = 0;

void cleanup(int tid) {
  std::cout << "Clean up tid " << tid << ".\n";
  auto &ts = states.at(tid);
  if (ts.serializer) {
    delete ts.serializer;
    ts.serializer = nullptr;
    std::cout << "Thread " << tid << " Traced #" << count << std::endl;
  }
}

void cleanup() {
  std::cout << "Clean up." << std::endl;
  for (int tid = 0; tid < MaxNThreads; ++tid) {
    cleanup(tid);
  }
}

// We simple close the file.
void switchFileImpl(int tid) { cleanup(tid); }

void printFuncEnterImpl(int tid, const char *FunctionName) {
  auto &protobufTraceEntry = states.at(tid).protobufTraceEntry;
  if (protobufTraceEntry.has_inst()) {
    // The previous one is inst.
    // Clear it and allocate a new func enter.
    protobufTraceEntry.clear_inst();
  }
  if (!protobufTraceEntry.has_func_enter()) {
    protobufTraceEntry.set_allocated_func_enter(
        new LLVM::TDG::DynamicLLVMFunctionEnter());
  }

  protobufTraceEntry.mutable_func_enter()->set_func(FunctionName);
}

void printInstImpl(int tid, uint64_t UID) {
  auto &protobufTraceEntry = states.at(tid).protobufTraceEntry;
  if (protobufTraceEntry.has_func_enter()) {
    // The previous one is func enter.
    // Clear it and allocate a new func enter.
    protobufTraceEntry.clear_func_enter();
  }
  if (!protobufTraceEntry.has_inst()) {
    protobufTraceEntry.set_allocated_inst(
        new LLVM::TDG::DynamicLLVMInstruction());
  }

  if (UID != 0) {
    protobufTraceEntry.mutable_inst()->set_uid(UID);
  } else {
    assert(false && "Invalid UID.");
  }
  protobufTraceEntry.mutable_inst()->clear_params();
  protobufTraceEntry.mutable_inst()->clear_result();
}

static const size_t VALUE_BUFFER_SIZE = 1024;
static char valueBuffer[VALUE_BUFFER_SIZE];
static void addValueToDynamicInst(int tid, const char Tag,
                                  const void *valueBuffer, size_t valueSize,
                                  bool isInt) {
  auto &protobufTraceEntry = states.at(tid).protobufTraceEntry;
  ::LLVM::TDG::DynamicLLVMValue *value = nullptr;
  switch (Tag) {
  case PRINT_VALUE_TAG_PARAMETER: {
    if (protobufTraceEntry.has_inst()) {
      value = protobufTraceEntry.mutable_inst()->add_params();
    } else {
      value = protobufTraceEntry.mutable_func_enter()->add_params();
    }
    break;
  }
  case PRINT_VALUE_TAG_RESULT: {
    // Only dynamic inst may have result.
    assert(protobufTraceEntry.has_inst());
    value = protobufTraceEntry.mutable_inst()->mutable_result();
    break;
  }
  default: {
    assert(false);
  }
  }
  if (isInt) {
    value->set_v_int(*(static_cast<const uint64_t *>(valueBuffer)));
  } else {
    value->set_v_bytes(valueBuffer, valueSize);
  }
}

void printValueLabelImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%s", Name);
  addValueToDynamicInst(tid, Tag, Name, strlen(Name), false);
}
void printValueIntImpl(int tid, const char Tag, const char *Name,
                       unsigned TypeId, uint64_t Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%" PRIu64, Value);
  addValueToDynamicInst(tid, Tag, &Value, sizeof(Value), true);
}
void printValueFloatImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId, double Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%f", Value);
  addValueToDynamicInst(tid, Tag, &Value, sizeof(Value), true);
}
void printValuePointerImpl(int tid, const char Tag, const char *Name,
                           unsigned TypeId, void *Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "0x%llx", (unsigned long
  // long)Value);
  addValueToDynamicInst(tid, Tag, &Value, sizeof(Value), true);
}
void printValueVectorImpl(int tid, const char Tag, const char *Name,
                          unsigned TypeId, uint32_t Size, uint8_t *Value) {
  addValueToDynamicInst(tid, Tag, Value, Size, false);
}
void printValueUnsupportImpl(int tid, const char Tag, const char *Name,
                             unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "UnsupportedType(%u)", TypeId);
  addValueToDynamicInst(tid, Tag, valueBuffer, strlen(valueBuffer), false);
}

// Serialize to file.
void printInstEndImpl(int tid) {
  auto &protobufTraceEntry = states.at(tid).protobufTraceEntry;
  assert(!protobufTraceEntry.has_func_enter() &&
         "Should not contain func enter for inst.");
  auto &trace = getTraceFile(tid);
  trace.serialize(protobufTraceEntry);
  count++;
  // std::cout << "printInstEnd" << std::endl;
}

void printFuncEnterEndImpl(int tid) {
  auto &protobufTraceEntry = states.at(tid).protobufTraceEntry;
  assert(!protobufTraceEntry.has_inst() &&
         "Should not contain inst for func enter.");
  if (count == 0) {
    std::cout << "The first one is func enter.\n";
  }
  auto &trace = getTraceFile(tid);
  trace.serialize(protobufTraceEntry);
  count++;
}