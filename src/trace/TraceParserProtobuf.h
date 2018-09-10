#ifndef LLVM_TDG_TRACE_PARSER_PROTOBUF_H
#define LLVM_TDG_TRACE_PARSER_PROTOBUF_H

#include "TraceMessage.pb.h"
#include "trace/InstructionUIDMap.h"
#include "trace/TraceParser.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <fstream>

class TraceParserProtobuf : public TraceParser {
public:
  TraceParserProtobuf(const std::string &TraceFileName,
                      const std::string &InstUIDMapFileName = "");
  ~TraceParserProtobuf();
  Type getNextType() override;
  TracedInst parseLLVMInstruction() override;
  TracedFuncEnter parseFunctionEnter() override;

private:
  std::ifstream TraceFile;
  google::protobuf::io::IstreamInputStream *IStream;
  LLVM::TDG::DynamicLLVMTraceEntry TraceEntry;

  InstructionUIDMap InstUIDMap;

  // Some statistics.
  uint64_t Count;

  void readNextEntry();
};

#endif