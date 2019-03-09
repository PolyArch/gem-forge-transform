#ifndef LLVM_TDG_TRACE_PARSER_PROTOBUF_H
#define LLVM_TDG_TRACE_PARSER_PROTOBUF_H

#include "TraceMessage.pb.h"
#include "trace/InstructionUIDMap.h"
#include "trace/TraceParser.h"

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <fstream>

class TraceParserProtobuf : public TraceParser {
 public:
  TraceParserProtobuf(const std::string &TraceFileName,
                      const InstructionUIDMap &_InstUIDMap);
  ~TraceParserProtobuf();
  Type getNextType() override;
  TracedInst parseLLVMInstruction() override;
  TracedFuncEnter parseFunctionEnter() override;

 private:
  std::ifstream TraceFile;
  google::protobuf::io::IstreamInputStream *IStream;
  google::protobuf::io::GzipInputStream *GzipIStream;
  google::protobuf::io::CodedInputStream *CodedIStream;
  LLVM::TDG::DynamicLLVMTraceEntry TraceEntry;

  const InstructionUIDMap &InstUIDMap;

  // Some statistics.
  uint64_t Count;

  void readNextEntry();

  /**
   * In order to not break user code, here we transform the value
   * to string.
   */
  std::string parseLLVMDynamicValueToString(
      const ::LLVM::TDG::DynamicLLVMValue &Value);
};

#endif