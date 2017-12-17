#include <stdarg.h>
#include <stdio.h>

void printFuncEnter(const char* FunctionName) {
  printf("e|%s|\n", FunctionName);
}

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  printf("i|%s|%s|%d|%s|\n", FunctionName, BBName, Id, OpCodeName);
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
void printValue(const char* Tag, const char* Name, const char* TypeName,
                unsigned TypeId, unsigned NumAdditionalArgs, ...) {
  printf("%s|%s|%s|%u|", Tag, Name, TypeName, TypeId);
  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
    case LabelTyID: {
      char* value = va_arg(VAList, char*);
      printf("%s", value);
      break;
    }
    case IntegerTyID: {
      unsigned value = va_arg(VAList, unsigned);
      printf("%u", value);
      break;
    }
    case DoubleTyID: {
      double value = va_arg(VAList, double);
      printf("%f", value);
      break;
    }
    case PointerTyID: {
      void* value = va_arg(VAList, void*);
      printf("%p", value);
      break;
    }
    default: {
      printf("UnsupportedType");
      break;
    }
  }
  va_end(VAList);
  printf("|\n");
}
