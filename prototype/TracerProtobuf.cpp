// Implement the tracer interface with protobuf.
#include "TraceMessage.pb.h"
#include "Tracer.h"

#include <cassert>
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

void cleanup() {
  std::ofstream& o = getTraceFile();
  o.close();
}

static LLVM::TDG::DynamicLLVMInstruction protobufInst;

void printFuncEnter(const char* FunctionName) {}

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  protobufInst.Clear();
  protobufInst.set_func(FunctionName);
  protobufInst.set_bb(BBName);
  protobufInst.set_id(Id);
}

void printValue(const char* Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {}

// Serialize to file.
void printInstEnd() { protobufInst.SerializeToOstream(&getTraceFile()); }