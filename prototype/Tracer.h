#ifndef LLVM_TDG_TRACER_H
#define LLVM_TDG_TRACER_H

// Interface for trace instrument.
// The implementation is provided as a static library.
extern const char* TRACE_FILE_NAME;

void printFuncEnter(const char* FunctionName);

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName);

void printValue(const char* Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...);

void printInstEnd();

#endif