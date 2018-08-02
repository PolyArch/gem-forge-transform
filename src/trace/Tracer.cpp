#include "trace/Tracer.h"
#include "trace/ProfileLogger.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
const char *DEFAULT_TRACE_FILE_NAME = "llvm_trace";
const char *getNewTraceFileName() {
  static size_t samples = 0;
  static char fileName[128];
  const char *traceFileName = std::getenv("LLVM_TDG_TRACE_FILE");
  if (traceFileName) {
    if (std::strlen(traceFileName) > 100) {
      assert(false && "Too long trace name.");
    }
  } else {
    traceFileName = DEFAULT_TRACE_FILE_NAME;
  }
  std::sprintf(fileName, "%s.%zu", traceFileName, samples);
  samples++;
  return fileName;
}

std::string getProfileFileName() {
  const char *traceFileName = std::getenv("LLVM_TDG_TRACE_FILE");
  if (!traceFileName) {
    traceFileName = DEFAULT_TRACE_FILE_NAME;
  }
  return std::string(traceFileName) + ".profile";
}

// This flag controls if I am already inside myself.
// This helps to solve the problem when the tracer runtime calls some functions
// which is also traced (so no recursion).
static bool insideMyself = false;
static uint64_t count;
static uint64_t tracedCount = 0;

/**
 * If START_INST is set (>0), the tracer will ignore the dynamic stack and
 * trace in a pattern of
 * [START_INST+MAX_INST*0+SKIP_INST*0 ... START_INST+MAX_INST*1+SKIP_INST*0] ...
 * [START_INST+MAX_INST*1+SKIP_INST*1 ... START_INST+MAX_INST*2+SKIP_INST*1] ...
 * ...
 * [START_INST+MAX_INST*i+SKIP_INST*i ... END_INST] ...
 *
 * Initialization is set to trace everything.
 */
static uint64_t START_INST = 0;
static uint64_t MAX_INST = 1;
static uint64_t SKIP_INST = 0;
static uint64_t END_INST = 0;

// In simple mode, it will not log the dynamic value of operands.
static bool isSimpleMode = false;

// The tracer will maintain a stack at run time to determine if the
// inst should be traced.
static std::vector<unsigned> stack;
static std::vector<const char *> stackName;
static unsigned tracedFunctionsInStack;
static std::string currentInstOpName;

// This serves as the guard.

static uint64_t getUint64Env(const char *name) {
  const char *env = std::getenv(name);
  if (env) {
    return std::stoull(std::string(env));
  } else {
    return 0;
  }
}

static void cleanup() {
  auto profileFileName = getProfileFileName();
  ProfileLogger::serializeToFile(profileFileName);
  /**
   * FIX IT!!
   * We really should deallocate this one, but it will cause
   * segment fault in the destructor and I have no idea how to fix it.
   * Since the memory will be recollected by the OS anyway, I leave it here.
   */
  printf("Done!\n");
}

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

    START_INST = getUint64Env("LLVM_TDG_START_INST");
    SKIP_INST = getUint64Env("LLVM_TDG_SKIP_INST");
    MAX_INST = getUint64Env("LLVM_TDG_MAX_INST");
    END_INST = getUint64Env("LLVM_TDG_END_INST");

    std::atexit(cleanup);

    printf("initializing skip inst to %lu...\n", SKIP_INST);
    printf("initializing max inst to %lu...\n", MAX_INST);
    printf("initializing start inst to %lu...\n", START_INST);
    printf("initializing end inst to %lu...\n", END_INST);
    initialized = true;
  }
}

/**
 * This functions decides if its time to trace.
 * 1. If START_INST is set (> 0), we just trace in the above pattern.
 * 2. Otherwise, we trace all the IsTraced function and their callees.
 */
static bool shouldLog() {
  if (START_INST > 0) {
    if (count >= START_INST) {
      return (count - START_INST) % (MAX_INST + SKIP_INST) < MAX_INST;
    } else {
      return false;
    }
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
  if (Id == 0) {
    // We are at the header of a basic block. Profile.
    std::string FuncStr(FunctionName);
    std::string BBStr(BBName);
    ProfileLogger::addBasicBlock(FuncStr, BBStr);
  }
  // printf("%s %s:%d, inside? %d count %lu\n", FunctionName, __FILE__,
  // __LINE__, insideMyself, count); Update current inst.
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
    // Check if we are about to switch file.
    if (START_INST > 0) {
      if ((count - START_INST) % (MAX_INST + SKIP_INST) == (MAX_INST - 1)) {
        // We are the last one of every MAX_INST. Switch file.
        switchFileImpl();
      }
    }
  }

  // Update the stack
  if (currentInstOpName == "ret") {
    // printf("%s\n", currentInstOpName.c_str());
    if (stack.size() == 0) {
      printf("Empty stack when we found ret?\n");
    }
    assert(stack.size() > 0);
    if (stack.back()) {
      tracedFunctionsInStack--;
    }
    stack.pop_back();
    stackName.pop_back();
  }

  // Check if we are about to exit.
  if (START_INST > 0) {
    if (END_INST > 0 && count == END_INST) {
      std::exit(0);
    }
  } else if (MAX_INST > 0) {
    if (tracedCount == MAX_INST) {
      std::exit(0);
    }
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