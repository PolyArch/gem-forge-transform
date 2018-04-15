#include "GZUtil.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static gzFile p = NULL;
static const char* TraceFileName = "llvm_trace.gz";
static const char* OpenMode = "w";

void cleanup() {
  if (p != NULL) {
    gzclose(p);
  }
}

#define log(...)                           \
  {                                        \
    if (p == NULL) {                       \
      p = gzopen(TraceFileName, OpenMode); \
      atexit(&cleanup);                    \
    }                                      \
    assert(p != NULL);                     \
    gzprintf(p, __VA_ARGS__);              \
  }

void printFuncEnter(const char* FunctionName) { log("e|%s|\n", FunctionName); }

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  log("i|%s|%s|%d|%s|\n", FunctionName, BBName, Id, OpCodeName);
}

// Copy from LLVM Type.h
enum LLVMTypeID {
  // PrimitiveTypes - make sure LastPrimitiveTyID stays up to date.
  VoidTyID = 0,   ///<  0: type with no size
  HalfTyID,       ///<  1: 16-bit floating point type
  FloatTyID,      ///<  2: 32-bit floating point type
  DoubleTyID,     ///<  3: 64-bit floating point type
  X86_FP80TyID,   ///<  4: 80-bit floating point type (X87)
  FP128TyID,      ///<  5: 128-bit floating point type (112-bit mantissa)
  PPC_FP128TyID,  ///<  6: 128-bit floating point type (two 64-bits, PowerPC)
  LabelTyID,      ///<  7: Labels
  MetadataTyID,   ///<  8: Metadata
  X86_MMXTyID,    ///<  9: MMX vectors (64 bits, X86 specific)
  TokenTyID,      ///< 10: Tokens

  // Derived types... see DerivedTypes.h file.
  // Make sure FirstDerivedTyID stays up to date!
  IntegerTyID,   ///< 11: Arbitrary bit width integers
  FunctionTyID,  ///< 12: Functions
  StructTyID,    ///< 13: Structures
  ArrayTyID,     ///< 14: Arrays
  PointerTyID,   ///< 15: Pointers
  VectorTyID     ///< 16: SIMD 'packed' format, or other vector type
};

/**
 * Print a value.
 * @param Tag: Can be either param or result.
 */
void printValue(const char* Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  log("%s|%s|%u|", Tag, Name, TypeId);
  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
    case LabelTyID: {
      // For label, log the name again to be compatible with other type.
      log("%s|", Name);
      break;
    }
    case IntegerTyID: {
      unsigned value = va_arg(VAList, unsigned);
      log("%u|", value);
      break;
    }
    case DoubleTyID: {
      double value = va_arg(VAList, double);
      log("%f|", value);
      break;
    }
    case PointerTyID: {
      void* value = va_arg(VAList, void*);
      log("%p|", value);
      break;
    }
    case VectorTyID: {
      uint32_t size = va_arg(VAList, uint32_t);
      uint8_t* buffer = va_arg(VAList, uint8_t*);
      for (uint32_t i = 0; i < size; ++i) {
        log("%hhu,", buffer[i]);
      }
      log("|");
      break;
    }
    default: {
      log("UnsupportedType|");
      break;
    }
  }
  va_end(VAList);
  log("\n");
}
