// Implement the tracer interface with protobuf.
#include "TraceMessage.pb.h"
#include "trace/Tracer.h"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>

void cleanup();

// Single thread singleton.
static std::ofstream o;
inline std::ofstream &getTraceFile() {
  if (!o.is_open()) {
    o.open(getNewTraceFileName(), std::ios::out | std::ios::binary);
    std::atexit(cleanup);
  }
  assert(o.is_open());
  return o;
}

static uint64_t count = 0;

void cleanup() {
  if (o.is_open()) {
    o.close();
    std::cout << "Traced #" << count << std::endl;
  }
}

// We simple close the file.
void switchFileImpl() { cleanup(); }

static LLVM::TDG::DynamicLLVMTraceEntry protobufTraceEntry;

void printFuncEnterImpl(const char *FunctionName) {
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

void printInstImpl(const char *FunctionName, const char *BBName, unsigned Id,
                   uint64_t UID, char *OpCodeName) {
  if (protobufTraceEntry.has_func_enter()) {
    // The previous one is func enter.
    // Clear it and allocate a new func enter.
    protobufTraceEntry.clear_func_enter();
  }
  if (!protobufTraceEntry.has_inst()) {
    protobufTraceEntry.set_allocated_inst(
        new LLVM::TDG::DynamicLLVMInstruction());
  }

  // std::cout << "Trace inst " << FunctionName << "::" << BBName
  //           << "::" << OpCodeName << std::endl;

  if (UID != 0) {
    protobufTraceEntry.mutable_inst()->set_uid(UID);
  } else {
    protobufTraceEntry.mutable_inst()->set_func(FunctionName);
    protobufTraceEntry.mutable_inst()->set_bb(BBName);
    protobufTraceEntry.mutable_inst()->set_id(Id);
  }
  protobufTraceEntry.mutable_inst()->set_op(OpCodeName);
  protobufTraceEntry.mutable_inst()->clear_params();
  protobufTraceEntry.mutable_inst()->clear_result();
}

static const size_t VALUE_BUFFER_SIZE = 1024;
static char valueBuffer[VALUE_BUFFER_SIZE];
static void addValueToDynamicInst(const char Tag) {
  switch (Tag) {
  case PRINT_VALUE_TAG_PARAMETER: {
    if (protobufTraceEntry.has_inst()) {
      // std::cout << "Add value to inst " << valueBuffer << std::endl;
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

void printValueLabelImpl(const char Tag, const char *Name, unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%s", Name);
  addValueToDynamicInst(Tag);
}
void printValueIntImpl(const char Tag, const char *Name, unsigned TypeId,
                       uint64_t Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%" PRIu64, Value);
  addValueToDynamicInst(Tag);
}
void printValueFloatImpl(const char Tag, const char *Name, unsigned TypeId,
                         double Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%f", Value);
  addValueToDynamicInst(Tag);
}
void printValuePointerImpl(const char Tag, const char *Name, unsigned TypeId,
                           void *Value) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "0x%llx", (unsigned long long)Value);
  addValueToDynamicInst(Tag);
}
void printValueVectorImpl(const char Tag, const char *Name, unsigned TypeId,
                          uint32_t Size, uint8_t *Value) {
  for (uint32_t i = 0, pos = 0; i < Size; ++i) {
    pos +=
        snprintf(valueBuffer + pos, VALUE_BUFFER_SIZE - pos, "%hhu,", Value[i]);
  }
  // std::cout << "Print vector " << Size << std::endl;
  addValueToDynamicInst(Tag);
}
void printValueUnsupportImpl(const char Tag, const char *Name,
                             unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "UnsupportedType(%u)", TypeId);
  addValueToDynamicInst(Tag);
}

// Serialize to file.
void printInstEndImpl() {
  assert(!protobufTraceEntry.has_func_enter() &&
         "Should not contain func enter for inst.");
  auto &trace = getTraceFile();
  uint64_t bytes = protobufTraceEntry.ByteSizeLong();
  trace.write(reinterpret_cast<char *>(&bytes), sizeof(bytes));
  protobufTraceEntry.SerializeToOstream(&trace);
  count++;
  // std::cout << "printInstEnd" << std::endl;
}

void printFuncEnterEndImpl() {
  assert(!protobufTraceEntry.has_inst() &&
         "Should not contain inst for func enter.");
  if (count == 0) {
    std::cout << "The first one is func enter.\n";
  }
  auto &trace = getTraceFile();
  uint64_t bytes = protobufTraceEntry.ByteSizeLong();
  trace.write(reinterpret_cast<char *>(&bytes), sizeof(bytes));
  protobufTraceEntry.SerializeToOstream(&trace);
  count++;
}