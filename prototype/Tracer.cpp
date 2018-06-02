#include "Tracer.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

// Contain some common definitions
const char* TRACE_FILE_NAME = "llvm_trace";

// The tracer will maintain a stack at run time to determine if the
// inst should be traced.
static uint64_t count;
static std::vector<unsigned> stack;
static std::vector<const char*> stackName;
static unsigned tracedFunctionsInStack;
static std::string currentInstOpName;

// This serves as the guard.
// We trace all the IsTraced function and their callees.
static bool shouldLog() { return tracedFunctionsInStack > 0; }

void printFuncEnter(const char* FunctionName, unsigned IsTraced) {
  // Update the stack.
  stack.push_back(IsTraced);
  stackName.push_back(FunctionName);
  if (stack.back()) {
    tracedFunctionsInStack++;
  }
  // printf("Entering %s, stack depth %lu\n", FunctionName, stack.size());
  if (shouldLog()) {
    printFuncEnterImpl(FunctionName);
  }
}

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  // Update current inst.
  currentInstOpName = OpCodeName;
  count++;
  const uint64_t PRINT_INTERVAL = 10000000;
  if (count % PRINT_INTERVAL < 5) {
    // Print every PRINT_INTERVAL instructions.
    printf("%lu:", count);
    for (size_t i = 0; i < stackName.size(); ++i) {
      printf("%s (%u) -> ", stackName[i], stack[i]);
    }
    printf("\n");
    if (count % PRINT_INTERVAL == 0) {
      printf("\n");
    }
  }
  if (shouldLog()) {
    printInstImpl(FunctionName, BBName, Id, OpCodeName);
  }
}

void printValue(const char Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  if (!shouldLog()) {
    return;
  }
  // In simplified mode, we ingore printValue call.
  const char* TraceMode = std::getenv("LLVM_TDG_TRACE_MODE");
  std::string SIMPLE = "SIMPLE";
  if (TraceMode && SIMPLE == TraceMode) {
    return;
  }

  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
    case TypeID::LabelTyID: {
      // For label, log the name again to be compatible with other type.
      printValueLabelImpl(Tag, Name, TypeId);
      break;
    }
    case TypeID::IntegerTyID: {
      uint64_t value = va_arg(VAList, uint64_t);
      printValueIntImpl(Tag, Name, TypeId, value);
      break;
    }
    // Float is promoted to double on x64.
    case TypeID::FloatTyID:
    case TypeID::DoubleTyID: {
      double value = va_arg(VAList, double);
      printValueFloatImpl(Tag, Name, TypeId, value);
      break;
    }
    case TypeID::PointerTyID: {
      void* value = va_arg(VAList, void*);
      printValuePointerImpl(Tag, Name, TypeId, value);
      break;
    }
    case TypeID::VectorTyID: {
      uint32_t size = va_arg(VAList, uint32_t);
      uint8_t* buffer = va_arg(VAList, uint8_t*);
      printValueVectorImpl(Tag, Name, TypeId, size, buffer);
      break;
    }
    default: {
      printValueUnsupportImpl(Tag, Name, TypeId);
      break;
    }
  }
  va_end(VAList);
}

void printInstEnd() {
  if (shouldLog()) {
    printInstEndImpl();
  }

  // Update the stack if this is a ret instruction.
  if (currentInstOpName == "ret") {
    // printf("%s\n", currentInstOpName.c_str());
    assert(stack.size() > 0);
    if (stack.back()) {
      tracedFunctionsInStack--;
    }
    stack.pop_back();
    stackName.pop_back();
  }
}

void printFuncEnterEnd() {
  if (shouldLog()) {
    printFuncEnterEndImpl();
  }
}