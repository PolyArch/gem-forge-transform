#ifndef LLVM_TDG_TRACER_H
#define LLVM_TDG_TRACER_H

#include <cstdarg>
#include <cstdint>

// Interface for trace instrument.
// The implementation is provided as a static library.

// Interface for the traced binary, so we need C ABI to avoid mangling names.
extern "C" {

/************************************************************************/
// Interface for the traced binary.

/**
 * Allocate data buffer, mainly used for data types that can't be passed in
 * through register, e.g. vector. Originally we allocate stack space for this,
 * but it results in stack overflow on deeply recursive benchmarks, e.g. gcc. So
 * we introduce this runtime helper.
 */
uint8_t *getOrAllocateDataBuffer(uint64_t size);

void printFuncEnter(const char *FunctionName, unsigned IsTraced);

void printInst(const char *FunctionName, uint64_t UID);

// Predefined tag:
// 'p' for parameter
// 'r' for result
#define PRINT_VALUE_TAG_PARAMETER (const char)('p')
#define PRINT_VALUE_TAG_RESULT (const char)('r')
void printValue(const char Tag, const char *Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...);

void printInstValue(unsigned NumAdditionalArgs, ...);

void printInstEnd();
void printFuncEnterEnd();
/***********************************************************************/
}

#endif