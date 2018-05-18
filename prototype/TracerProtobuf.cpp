// Implement the tracer interface with protobuf.
#include "TraceMessage.pb.h"
#include "Tracer.h"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>

void cleanup();

// Single thread singleton.
inline std::ofstream& getTraceFile() {
  static std::ofstream o;
  if (!o.is_open()) {
    o.open(TRACE_FILE_NAME);
    std::atexit(cleanup);
  }
  assert(o.is_open());
  return o;
}

static uint64_t count = 0;

void cleanup() {
  std::ofstream& o = getTraceFile();
  o.close();
  std::cout << "Traced #" << count << std::endl;
}

static LLVM::TDG::DynamicLLVMTraceEntry protobufTraceEntry;

void printFuncEnterImpl(const char* FunctionName) {
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

void printInstImpl(const char* FunctionName, const char* BBName, unsigned Id,
                   char* OpCodeName) {
  if (protobufTraceEntry.has_func_enter()) {
    // The previous one is func enter.
    // Clear it and allocate a new func enter.
    protobufTraceEntry.clear_func_enter();
  }
  if (!protobufTraceEntry.has_inst()) {
    protobufTraceEntry.set_allocated_inst(
        new LLVM::TDG::DynamicLLVMInstruction());
  }

  protobufTraceEntry.mutable_inst()->set_func(FunctionName);
  protobufTraceEntry.mutable_inst()->set_bb(BBName);
  protobufTraceEntry.mutable_inst()->set_id(Id);
  protobufTraceEntry.mutable_inst()->clear_params();
  protobufTraceEntry.mutable_inst()->clear_result();
}

static const size_t VALUE_BUFFER_SIZE = 256;
static char valueBuffer[VALUE_BUFFER_SIZE];
static void addValueToDynamicInst(const char Tag) {
  switch (Tag) {
    case PRINT_VALUE_TAG_PARAMETER: {
      if (protobufTraceEntry.has_inst()) {
        protobufTraceEntry.mutable_inst()->add_params(valueBuffer);
      } else {
        protobufTraceEntry.mutable_func_enter()->add_params(valueBuffer);
      }
      break;
    }
    case PRINT_VALUE_TAG_RESULT: {
      // Only dynamic inst may have result.
      assert(protobufTraceEntry.has_inst());
      protobufTraceEntry.mutable_inst()->set_result(valueBuffer);
      break;
    }
    default: { assert(false); }
  }
}

void printValueLabelImpl(const char Tag, const char* Name, unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%s", Name);
  addValueToDynamicInst(Tag);
}
void printValueIntImpl(const char Tag, const char* Name, unsigned TypeId,
                       uint64_t Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%" PRIu64, Value);
  addValueToDynamicInst(Tag);
}
void printValueFloatImpl(const char Tag, const char* Name, unsigned TypeId,
                         double Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%f", Value);
  addValueToDynamicInst(Tag);
}
void printValuePointerImpl(const char Tag, const char* Name, unsigned TypeId,
                           void* Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%p", Value);
  addValueToDynamicInst(Tag);
}
void printValueVectorImpl(const char Tag, const char* Name, unsigned TypeId,
                          uint32_t Size, uint8_t* Value) {
  for (uint32_t i = 0, pos = 0; i < Size; ++i) {
    pos +=
        snprintf(valueBuffer + pos, VALUE_BUFFER_SIZE - pos, "%hhu,", Value[i]);
  }
  addValueToDynamicInst(Tag);
}
void printValueUnsupportImpl(const char Tag, const char* Name,
                             unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "UnsupportedType(%u)", TypeId);
  addValueToDynamicInst(Tag);
}

// Serialize to file.
void printInstEndImpl() {
  protobufTraceEntry.SerializeToOstream(&getTraceFile());
  count++;
}

void printFuncEnterEndImpl() {
  protobufTraceEntry.SerializeToOstream(&getTraceFile());
  count++;
}