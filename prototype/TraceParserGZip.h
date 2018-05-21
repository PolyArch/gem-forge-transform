#ifndef LLVM_TDG_TRACE_PARSER_GZIP_H
#define LLVM_TDG_TRACE_PARSER_GZIP_H

#include "GZipUtil.h"
#include "TraceParser.h"

class TraceParserGZip : public TraceParser {
 public:
  TraceParserGZip(const std::string& TraceFileName);
  ~TraceParserGZip();
  Type getNextType() override;
  TracedInst parseLLVMInstruction() override;
  TracedFuncEnter parseFunctionEnter() override;

 private:
  GZTraceT* TraceFile;
  void readLine();

  static bool isBreakLine(const char* Line, int LineLen);

  /**
   * Split a string like a|b|c| into [a, b, c].
   */
  static std::vector<std::string> splitByChar(const std::string& source,
                                              char split);
};

#endif