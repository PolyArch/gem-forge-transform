// Implement the tracer interface with protobuf.
#include "TraceMessage.pb.h"
#include "Tracer.h"

#include <cassert>
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

void printFuncEnter(const char* FunctionName) {
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

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  if (protobufTraceEntry.has_func_enter()) {
    // The previous one is inst.
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

void printValue(const char Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  const size_t VALUE_BUFFER_SIZE = 256;
  static char valueBuffer[VALUE_BUFFER_SIZE];

  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
    case TypeID::LabelTyID: {
      // For label, log the name again to be compatible with other type.
      snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%s", Name);
      break;
    }
    case TypeID::IntegerTyID: {
      unsigned value = va_arg(VAList, unsigned);
      snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%u", value);
      break;
    }
    // Float is promoted to double on x64.
    case TypeID::FloatTyID:
    case TypeID::DoubleTyID: {
      double value = va_arg(VAList, double);
      snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%f", value);
      break;
    }
    case TypeID::PointerTyID: {
      void* value = va_arg(VAList, void*);
      snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%p", value);
      break;
    }
    case TypeID::VectorTyID: {
      uint32_t size = va_arg(VAList, uint32_t);
      uint8_t* buffer = va_arg(VAList, uint8_t*);
      for (uint32_t i = 0; i < size; ++i) {
        snprintf(valueBuffer, VALUE_BUFFER_SIZE, "%hhu,", buffer[i]);
      }
      break;
    }
    default: {
      snprintf(valueBuffer, VALUE_BUFFER_SIZE, "UnsupportedType");
      break;
    }
  }
  va_end(VAList);

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

// Serialize to file.
void printInstEnd() {
  protobufTraceEntry.SerializeToOstream(&getTraceFile());
  count++;
}