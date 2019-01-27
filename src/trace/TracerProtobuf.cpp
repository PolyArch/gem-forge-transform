// Implement the tracer interface with protobuf.
#include "TraceMessage.pb.h"
#include "trace/Tracer.h"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

void cleanup();

// Single thread singleton.
static std::ofstream o;
static google::protobuf::io::OstreamOutputStream *fStream = nullptr;
static google::protobuf::io::GzipOutputStream *gzipStream = nullptr;
static google::protobuf::io::CodedOutputStream *codedStream = nullptr;
inline google::protobuf::io::CodedOutputStream &getTraceFile() {
  if (!o.is_open()) {
    o.open(getNewTraceFileName(), std::ios::out | std::ios::binary);
    std::atexit(cleanup);
    fStream = new google::protobuf::io::OstreamOutputStream(&o);
    gzipStream = new google::protobuf::io::GzipOutputStream(fStream);
    codedStream = new google::protobuf::io::CodedOutputStream(gzipStream);
  }
  assert(o.is_open());
  assert(fStream != nullptr);
  assert(gzipStream != nullptr);
  assert(codedStream != nullptr);
  return *codedStream;
}

static uint64_t count = 0;

void cleanup() {
  std::cout << "Clean up." << std::endl;
  if (o.is_open()) {
    delete codedStream;
    codedStream = nullptr;
    gzipStream->Close();
    delete gzipStream;
    gzipStream = nullptr;
    delete fStream;
    fStream = nullptr;
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
static void addValueToDynamicInst(const char Tag, const void *valueBuffer,
                                  size_t valueSize, bool isInt) {
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
    default: { assert(false); }
  }
  if (isInt) {
    value->set_v_int(*(static_cast<const uint64_t *>(valueBuffer)));
  } else {
    value->set_v_bytes(valueBuffer, valueSize);
  }
}

void printValueLabelImpl(const char Tag, const char *Name, unsigned TypeId) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%s", Name);
  addValueToDynamicInst(Tag, Name, strlen(Name), false);
}
void printValueIntImpl(const char Tag, const char *Name, unsigned TypeId,
                       uint64_t Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%" PRIu64, Value);
  addValueToDynamicInst(Tag, &Value, sizeof(Value), true);
}
void printValueFloatImpl(const char Tag, const char *Name, unsigned TypeId,
                         double Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%f", Value);
  addValueToDynamicInst(Tag, &Value, sizeof(Value), true);
}
void printValuePointerImpl(const char Tag, const char *Name, unsigned TypeId,
                           void *Value) {
  // snprintf(valueBuffer, VALUE_BUFFER_SIZE, "0x%llx", (unsigned long
  // long)Value);
  addValueToDynamicInst(Tag, &Value, sizeof(Value), true);
}
void printValueVectorImpl(const char Tag, const char *Name, unsigned TypeId,
                          uint32_t Size, uint8_t *Value) {
  addValueToDynamicInst(Tag, Value, Size, false);
}
void printValueUnsupportImpl(const char Tag, const char *Name,
                             unsigned TypeId) {
  snprintf(valueBuffer, VALUE_BUFFER_SIZE, "UnsupportedType(%u)", TypeId);
  addValueToDynamicInst(Tag, valueBuffer, strlen(valueBuffer), false);
}

// Serialize to file.
void printInstEndImpl() {
  assert(!protobufTraceEntry.has_func_enter() &&
         "Should not contain func enter for inst.");
  auto &trace = getTraceFile();
  trace.WriteVarint32(protobufTraceEntry.ByteSize());
  protobufTraceEntry.SerializeWithCachedSizes(&trace);
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
  trace.WriteVarint32(protobufTraceEntry.ByteSize());
  protobufTraceEntry.SerializeWithCachedSizes(&trace);
  count++;
}