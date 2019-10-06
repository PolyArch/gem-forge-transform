#ifndef GEM_FORGE_TRACER_IMPL_H
#define GEM_FORGE_TRACER_IMPL_H

#include <cstdint>

static const size_t MaxNThreads = 10;

/**
 * Allocate a new trace file name, starting from "xxx.trace.0"
 */
const char *getNewTraceFileName(int tid);

// Library implemented.
void printFuncEnterImpl(int tid, const char *FunctionName);

void printInstImpl(int tid, const char *FunctionName, const char *BBName,
                   unsigned Id, uint64_t UID, const char *OpCodeName);

// Predefined tag:
// 'p' for parameter
// 'r' for result
#define PRINT_VALUE_TAG_PARAMETER (const char)('p')
#define PRINT_VALUE_TAG_RESULT (const char)('r')
void printValueLabelImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId);
void printValueIntImpl(int tid, const char Tag, const char *Name,
                       unsigned TypeId, uint64_t Value);
void printValueFloatImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId, double Value);
void printValuePointerImpl(int tid, const char Tag, const char *Name,
                           unsigned TypeId, void *Value);
void printValueVectorImpl(int tid, const char Tag, const char *Name,
                          unsigned TypeId, uint32_t Size, uint8_t *Value);
void printValueUnsupportImpl(int tid, const char Tag, const char *Name,
                             unsigned TypeId);

void printInstEndImpl(int tid);
void printFuncEnterEndImpl(int tid);

/**
 * In sampling trace mode, we create a new trace file for different sample.
 * Library should call getNewTraceFileName to get the file name for next sample.
 */
void switchFileImpl(int tid);

#endif