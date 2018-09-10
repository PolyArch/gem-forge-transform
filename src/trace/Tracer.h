#ifndef LLVM_TDG_TRACER_H
#define LLVM_TDG_TRACER_H

#include <cstdarg>
#include <cstdint>

// Interface for trace instrument.
// The implementation is provided as a static library.

// Interface for the traced binary, so we need C ABI to avoid mangling names.
extern "C" {

/**
 * Allocate a new trace file name, starting from "xxx.trace.0"
 */
const char *getNewTraceFileName();

/************************************************************************/
// Interface for the traced binary.
void printFuncEnter(const char *FunctionName, unsigned IsTraced);

void printInst(const char *FunctionName, const char *BBName, unsigned Id,
               uint64_t UID, char *OpCodeName);

// Predefined tag:
// 'p' for parameter
// 'r' for result
#define PRINT_VALUE_TAG_PARAMETER (const char)('p')
#define PRINT_VALUE_TAG_RESULT (const char)('r')
void printValue(const char Tag, const char *Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...);

void printInstEnd();
void printFuncEnterEnd();
/***********************************************************************/
}

// Library implemented.
void printFuncEnterImpl(const char *FunctionName);

void printInstImpl(const char *FunctionName, const char *BBName, unsigned Id,
                   uint64_t UID, char *OpCodeName);

// Predefined tag:
// 'p' for parameter
// 'r' for result
#define PRINT_VALUE_TAG_PARAMETER (const char)('p')
#define PRINT_VALUE_TAG_RESULT (const char)('r')
void printValueLabelImpl(const char Tag, const char *Name, unsigned TypeId);
void printValueIntImpl(const char Tag, const char *Name, unsigned TypeId,
                       uint64_t Value);
void printValueFloatImpl(const char Tag, const char *Name, unsigned TypeId,
                         double Value);
void printValuePointerImpl(const char Tag, const char *Name, unsigned TypeId,
                           void *Value);
void printValueVectorImpl(const char Tag, const char *Name, unsigned TypeId,
                          uint32_t Size, uint8_t *Value);
void printValueUnsupportImpl(const char Tag, const char *Name, unsigned TypeId);

void printInstEndImpl();
void printFuncEnterEndImpl();

/**
 * In sampling trace mode, we create a new trace file for different sample.
 * Library should call getNewTraceFileName to get the file name for next sample.
 */
void switchFileImpl();

#endif