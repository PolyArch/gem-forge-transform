#include "trace/Tracer.h"
#include "trace/ProfileLogger.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

// The tracer will work with two environment variables:
// 1. LLVM_TDG_TRACE_MODE, if simple, it will not trace value, but instructions
// only.
// 2. LLVM_TDG_TRACE_MAXIMUM_INST, if specified, it will exit after tracting
// this number of instructions.

// Copied from llvm/IR/Type.h.
// Definitely not the best way to do thes, but since it barely changes
// and imitiveTypes - make sure LastPrimitiveTyID stays up to date.
enum TypeID {
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
const char *DEFAULT_TRACE_FILE_NAME = "llvm";
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
  std::sprintf(fileName, "%s.%zu.trace", traceFileName, samples);
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

namespace {

// This flag controls if I am already inside myself.
// This helps to solve the problem when the tracer runtime calls some functions
// which is also traced (so no recursion).
static bool insideMyself = false;
static uint64_t count = 0;
static uint64_t countInTraceFunc = 0;
static uint64_t tracedCount = 0;

// Use this flag to ingore all the initialization phase.
static bool hasSeenMain = false;

static bool isDebug = false;

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

/**
 * Determins how to interpret the START_INST and so on.
 */
static bool MEASURE_IN_TRACE_FUNC = false;

static uint64_t PRINT_INTERVAL = 10000000;

// In simple mode, it will not log the dynamic value of operands.
static bool isSimpleMode = false;

// The tracer will maintain a stack at run time to determine if the
// inst should be traced.
static std::vector<unsigned> stack;
static std::vector<const char *> stackName;
static unsigned tracedFunctionsInStack;
static const char *currentInstOpName = nullptr;

// Global profile log.
static ProfileLogger allProfile;
}  // namespace

// This serves as the guard.

static uint64_t getUint64Env(const char *name) {
  const char *env = std::getenv(name);
  if (env) {
    return std::stoull(std::string(env));
  } else {
    return 0;
  }
}

static bool getBoolEnv(const char *name) {
  const char *env = std::getenv(name);
  if (env) {
    return std::strcmp(env, "true") == 0;
  } else {
    return false;
  }
}

static void cleanup() {
  auto profileFileName = getProfileFileName();
  allProfile.serializeToFile(profileFileName);
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

    const char *meaureInTraceFunc =
        std::getenv("LLVM_TDG_MEASURE_IN_TRACE_FUNC");
    std::string MEASURE_IN_TRACE_FUNC_VAL = "TRUE";
    if (meaureInTraceFunc != nullptr) {
      MEASURE_IN_TRACE_FUNC = MEASURE_IN_TRACE_FUNC_VAL == meaureInTraceFunc;
    } else {
      MEASURE_IN_TRACE_FUNC = false;
    }

    PRINT_INTERVAL = getUint64Env("LLVM_TDG_PRINT_INTERVAL");
    if (PRINT_INTERVAL == 0) {
      PRINT_INTERVAL = 10000000;
    }

    isDebug = getBoolEnv("LLVM_TDG_DEBUG");

    std::atexit(cleanup);

    printf("initializing SKIP_INST to %lu...\n", SKIP_INST);
    printf("initializing MAX_INST to %lu...\n", MAX_INST);
    printf("initializing START_INST to %lu...\n", START_INST);
    printf("initializing END_INST to %lu...\n", END_INST);
    printf("initializing PRINT_INTERVAL to %lu...\n", PRINT_INTERVAL);
    printf("initializing isDebug to %d...\n", isDebug);
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
    auto c = count;
    if (MEASURE_IN_TRACE_FUNC) {
      c = countInTraceFunc;
    }
    if (c >= START_INST) {
      return (c - START_INST) % (MAX_INST + SKIP_INST) < MAX_INST;
    } else {
      return false;
    }
  } else {
    return tracedFunctionsInStack > 0;
  }
}

static void printStack() {
  for (size_t i = 0; i < stackName.size(); ++i) {
    printf("%s (%u) -> ", stackName[i], stack[i]);
  }
  printf("\n");
}

// Try to use libunwind to unwind the stack to some point. Return if succeed.
static void unwind() {
  unw_cursor_t cursor;
  unw_context_t context;

  unw_getcontext(&context);
  unw_init_local(&cursor, &context);

  std::vector<std::string> unwinded;

  while (unw_step(&cursor) > 0) {
    unw_word_t offset, pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0) {
      break;
    }
    char sym[256];
    if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
      unwinded.push_back(sym);
      if (isDebug) {
        printf("0x%lx: (%s+0x%lx)\n", pc, sym, offset);
      }
    } else {
      if (isDebug) {
        printf("0x%lx: -- unable to get the symbol\n", pc);
      }
      // Ignore the frame without symbol name
    }
  }

  // Matching algorithm:
  // For every frame in our stack, search in the unwinded stack, if there is no
  // match, then we assume this is the boundary.
  assert(unwinded.size() > 0 && "There should be at least one frame.");
  int boundary = 0;
  int unwindedIdx = unwinded.size() - 1;
  while (boundary < stackName.size()) {
    bool found = false;
    while (unwindedIdx >= 0 && !found) {
      if (stackName[boundary] == unwinded[unwindedIdx]) {
        // Found a match.
        boundary++;
        found = true;
      }
      unwindedIdx--;
    }
    if (!found) {
      // This is the boundary.
      break;
    }
  }

  // Throw away the frame beyond the boundary.
  stackName.resize(boundary);
  stack.resize(boundary);
}

void printFuncEnter(const char *FunctionName, unsigned IsTraced) {
  // printf("%s %s:%d, inside? %d\n", FunctionName, __FILE__, __LINE__,
  // insideMyself);
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  if (!hasSeenMain) {
    if (std::strcmp(FunctionName, "main") == 0) {
      // Keep going.
      hasSeenMain = true;
    } else {
      // Exit.
      insideMyself = false;
      return;
    }
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
               uint64_t UID, char *OpCodeName) {
  if (insideMyself) {
    return;
  } else {
    insideMyself = true;
  }
  if (!hasSeenMain) {
    if (std::strcmp(FunctionName, "main") == 0) {
      // Keep going.
      hasSeenMain = true;
    } else {
      // Exit.
      insideMyself = false;
      return;
    }
  }
  initialize();
  if (stackName.empty()) {
    printf("Empty stack when we in printInst?.");
    std::exit(1);
  }

  // Check if this is a landingpad instruction.
  if (std::strcmp(OpCodeName, "landingpad") == 0) {
    // This is a landingpad instruction.
    // Use libunwind to adjust our stack.
    if (isDebug) {
      printf("Before unwind the stack: \n");
      printStack();
    }
    unwind();
    assert(!stackName.empty() && "After unwind the stack is empty.");

    if (isDebug) {
      printf("After unwind the stack: \n");
      printStack();
    }
  }

  if (stackName.back() != FunctionName) {
    printf("Unmatched FunctionName %s %s %u with stack: \n", FunctionName,
           BBName, Id);
    printStack();
    std::exit(1);
  }
  if (Id == 0) {
    // We are at the header of a basic block. Profile.
    std::string FuncStr(FunctionName);
    std::string BBStr(BBName);
    allProfile.addBasicBlock(FuncStr, BBStr);
  }
  // printf("%s %s:%d, inside? %d count %lu\n", FunctionName, __FILE__,
  // __LINE__, insideMyself, count); Update current inst.
  assert(currentInstOpName == nullptr && "Previous inst has not been closed.");
  currentInstOpName = OpCodeName;
  count++;
  if (tracedFunctionsInStack > 0) {
    countInTraceFunc++;
  }
  if (count % PRINT_INTERVAL < 5) {
    // Print every PRINT_INTERVAL instructions.
    printf("%lu:", count);
    for (size_t i = 0; i < stackName.size(); ++i) {
      printf("%s (%u) -> ", stackName[i], stack[i]);
    }
    printf(" %s %u %s\n", BBName, Id, OpCodeName);
    if (count % PRINT_INTERVAL == 0) {
      printf("\n");
    }
  }
  if (shouldLog()) {
    printInstImpl(FunctionName, BBName, Id, UID, OpCodeName);
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
  if (!hasSeenMain) {
    insideMyself = false;
    return;
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
  if (!hasSeenMain) {
    insideMyself = false;
    return;
  }
  if (shouldLog()) {
    tracedCount++;
    printInstEndImpl();
    // Check if we are about to switch file.
    auto c = count;
    if (MEASURE_IN_TRACE_FUNC) {
      c = countInTraceFunc;
    }
    if (START_INST > 0) {
      if ((c - START_INST) % (MAX_INST + SKIP_INST) == (MAX_INST - 1)) {
        // We are the last one of every MAX_INST. Switch file.
        switchFileImpl();
      }
    }
  }

  // Update the stack
  assert(currentInstOpName != nullptr &&
         "Missing currentInstOpName in printInstEnd.");
  if (std::strcmp(currentInstOpName, "ret") == 0) {
    // printf("%s\n", currentInstOpName.c_str());
    if (isDebug) {
      printf("Return from: ");
      printStack();
    }
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
  currentInstOpName = nullptr;

  // Check if we are about to exit.
  auto c = count;
  if (MEASURE_IN_TRACE_FUNC) {
    c = countInTraceFunc;
  }
  if (START_INST > 0) {
    if (END_INST > 0 && c == END_INST) {
      std::exit(0);
    }
  } else if (END_INST > 0) {
    if (c == END_INST) {
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
  if (!hasSeenMain) {
    insideMyself = false;
    return;
  }
  if (shouldLog()) {
    printFuncEnterEndImpl();
  }
  insideMyself = false;
  return;
}