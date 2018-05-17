#ifndef LLVM_TDG_TRACER_H
#define LLVM_TDG_TRACER_H

#include <cstdarg>

// Interface for trace instrument.
// The implementation is provided as a static library.
extern "C" {

// Copied from llvm/IR/Type.h.
// Definitely not the best way to do this, but since it barely changes
// and we don't want to link llvm to our trace library...
enum TypeID {
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

extern const char* TRACE_FILE_NAME;

void printFuncEnter(const char* FunctionName);

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName);

// Predefined tag:
// 'p' for parameter
// 'r' for result
#define PRINT_VALUE_TAG_PARAMETER (const char)('p')
#define PRINT_VALUE_TAG_RESULT (const char)('r')
void printValue(const char Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...);

void printInstEnd();
}

#endif