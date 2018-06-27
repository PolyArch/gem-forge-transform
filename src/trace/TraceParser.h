
#ifndef LLVM_TDG_TRACE_PARSER_H
#define LLVM_TDG_TRACE_PARSER_H

#include <string>
#include <vector>

class TraceParser {
 public:
  enum Type {
    INST,
    FUNC_ENTER,
    END,
  };
  struct TracedInst {
    std::string Func;
    std::string BB;
    unsigned Id;
    std::string Op;
    std::vector<std::string> Operands;
    std::string Result;
  };
  struct TracedFuncEnter {
    std::string Func;
    std::vector<std::string> Arguments;
  };
  virtual ~TraceParser() {}
  virtual Type getNextType() = 0;
  virtual TracedInst parseLLVMInstruction() = 0;
  virtual TracedFuncEnter parseFunctionEnter() = 0;
};

#endif