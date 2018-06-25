#include "Tracer.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// The tracer will work with two environment variables:
// 1. LLVM_TDG_TRACE_MODE, if simple, it will not trace value, but instructions
// only.
// 2. LLVM_TDG_TRACE_MAXIMUM_INST, if specified, it will exit after tracting
// this number of instructions.

// Copied from llvm/IR/Type.h.
// Definitely not the best way to do thes, but since it barely changes
// and imitiveTypes - make sure LastPrimitiveTyID stays up to date.
enum TypeID {
  VoidTyID = 0,  ///<  0: type with no size
  HalfTyID,      ///<  1: 16-bit floating point type
  FloatTyID,     ///<  2: 32-bit floating point type
  DoubleTyID,    ///<  3: 64-bit floating point type
  X86_FP80TyID,  ///<  4: 80-bit floating point type (X87)
  FP128TyID,     ///<  5: 128-bit floating point type (112-bit mantissa)
  PPC_FP128TyID, ///<  6: 128-bit floating point type (two 64-bits, PowerPC)
  LabelTyID,     ///<  7: Labels
  MetadataTyID,  ///<  8: Metadata
  X86_MMXTyID,   ///<  9: MMX vectors (64 bits, X86 specific)
  TokenTyID,     ///< 10: Tokens

  // Derived types... see DerivedTypes.h file.
  // Make sure FirstDerivedTyID stays up to date!
  IntegerTyID,  ///< 11: Arbitrary bit width integers
  FunctionTyID, ///< 12: Functions
  StructTyID,   ///< 13: Structures
  ArrayTyID,    ///< 14: Arrays
  PointerTyID,  ///< 15: Pointers
  VectorTyID    ///< 16: SIMD 'packed' format, or other vector type
};

// Contain some common definitions
const char *TRACE_FILE_NAME = "llvm_trace";

// This flag controls if I am already inside myself.
// This helps to solve the problem when the runtime called some function
// which is also traced (so no recursion).
static bool insideMyself = false;
static uint64_t count;
static uint64_t tracedCount = 0;
static uint64_t SKIP_INST = 0;
static uint64_t MAXIMUM_TRACED_INST = 0;
static bool isSimpleMode = false;

// The tracer will maintain a stack at run time to determine if the
// inst should be traced.
static std::vector<unsigned> stack;
static std::vector<const char *> stackName;
static unsigned tracedFunctionsInStack;
static std::string currentInstOpName;

// This serves as the guard.

static void initialize() {
  static bool initialized = false;
  if (!initialized) {
    printf("initializing tracer...\n");
    // Set the trace mode.
    const char *TraceMode = std::getenv("LLVM_TDG_TRACE_MODE");
    std::string SIMPLE = "SIMPLE";
    if (TraceMode) {
      isSimpleMode = SIMPLE == TraceMode;
    } else {
      isSimpleMode = false;
    }

    // Set the maximum trace number.
    const char *MaximumInst = std::getenv("LLVM_TDG_MAXIMUM_INST");
    if (MaximumInst) {
      MAXIMUM_TRACED_INST = std::stoull(std::string(MaximumInst));
    }

    // Set the skipped trace number.
    const char *SkipInst = std::getenv("LLVM_TDG_SKIP_INST");
    if (SkipInst) {
      SKIP_INST = std::stoull(std::string(SkipInst));
    }

    printf("initializing skip inst to %lu...\n", SKIP_INST);
    printf("initializing maximum inst to %lu...\n", MAXIMUM_TRACED_INST);
    initialized = true;
  }
}

/**
 * This functions decides if its time to trace.
 * 1. If SKIP_INST is set (> 0), we just wait until count > SKIP_INST.
 * 2. Otherwise, we trace all the IsTraced function and their callees.
 */
static bool shouldLog() {
  if (SKIP_INST > 0) {
    return count > SKIP_INST;
  } else {
    return tracedFunctionsInStack > 0;
  }
}

void printFuncEnter(const char *FunctionName, unsigned IsTraced) {
  // printf("%s %s:%d, inside? %d\n", FunctionName, __FILE__, __LINE__,
  // insideMyself);
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  // printf("%s:%d, inside? %d\n", __FILE__, __LINE__, insideMyself);
  initialize();
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
  insideMyself = false;
  // printf("%s:%d, inside? %d\n", __FILE__, __LINE__, insideMyself);
  return;
}

void printInst(const char *FunctionName, const char *BBName, unsigned Id,
               char *OpCodeName) {
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  initialize();
  // printf("%s %s:%d, inside? %d count %lu\n", FunctionName, __FILE__, __LINE__, insideMyself, count);
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
  insideMyself = false;
  return;
}

void printValue(const char Tag, const char *Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  if (!shouldLog()) {
    insideMyself = false;
    return;
  }
  // In simplified mode, we ingore printValue call.
  if (isSimpleMode) {
    insideMyself = false;
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
    void *value = va_arg(VAList, void *);
    printValuePointerImpl(Tag, Name, TypeId, value);
    break;
  }
  case TypeID::VectorTyID: {
    uint32_t size = va_arg(VAList, uint32_t);
    uint8_t *buffer = va_arg(VAList, uint8_t *);
    printValueVectorImpl(Tag, Name, TypeId, size, buffer);
    break;
  }
  default: {
    printValueUnsupportImpl(Tag, Name, TypeId);
    break;
  }
  }
  va_end(VAList);
  insideMyself = false;
  return;
}

void printInstEnd() {
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  if (shouldLog()) {
    tracedCount++;
    printInstEndImpl();
    // Check if we are about to exit.
    if (MAXIMUM_TRACED_INST > 0 && tracedCount == MAXIMUM_TRACED_INST) {
      // We have reached the limit.
      // Simply exit.
      std::exit(0);
    }
  }

  // Update the stack
  if (currentInstOpName == "ret") {
    // printf("%s\n", currentInstOpName.c_str());
    assert(stack.size() > 0);
    if (stack.back()) {
      tracedFunctionsInStack--;
    }
    stack.pop_back();
    stackName.pop_back();
  }
  insideMyself = false;
  return;
}

void printFuncEnterEnd() {
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  if (shouldLog()) {
    printFuncEnterEndImpl();
  }
  insideMyself = false;
  return;
}