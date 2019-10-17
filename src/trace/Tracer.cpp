#include "Tracer.h"
#include "InstructionUIDMapReader.h"
#include "ProfileLogger.h"
#include "TracerImpl.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <list>
#include <pthread.h>
#include <sstream>
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

namespace {

static InstructionUIDMapReader instUIDMap;

// Use this flag to ingore all the initialization phase.
static bool hasSeenMain = false;

static bool isDebug = false;

/**
 * Specify the ROI of the tracer.
 * 1. All
 *    -> All instructions starting from main.
 * 2. SpecifiedFunction
 *    -> Only specified functions and their callee are considered by the tracer.
 *       So far such functions are specified by the printFunctionEnter().
 */
enum TraceROI {
  All = 0,
  SpecifiedFunction = 1,
};

/**
 * Specify the mode of the tracer.
 * 1. Profile
 *    -> All basic blocks in ROI are profiled.
 * 2. TraceAll
 *    -> All ROI instructions are traced.
 * 3. TraceUniformInterval
 *    -> ROI Instructions in uniformly spaced intervals are traced.
 * 4. TraceSpecifiedInterval
 *    -> ROI Instructions in user-specified intervals are traced.
 */
enum TraceMode {
  Profile = 0,
  TraceAll = 1,
  TraceUniformInterval = 2,
  TraceSpecifiedInterval = 3,
};

static TraceROI traceROI = TraceROI::All;
static TraceMode traceMode = TraceMode::Profile;
static const char *DEFAULT_TRACE_FOLDER = "llvm";
static const char *traceFolder = DEFAULT_TRACE_FOLDER;

/********************************************************************
 * Parameters for Profile mode.
 *******************************************************************/

/********************************************************************
 * Parameters for TraceUniformInterval mode.
 *******************************************************************/

/**
 * If START_INST is set (>0), the tracer will trace in a pattern of
 * [START_INST+MAX_INST*0+SKIP_INST*0 ... START_INST+MAX_INST*1+SKIP_INST*0] ...
 * [START_INST+MAX_INST*1+SKIP_INST*1 ... START_INST+MAX_INST*2+SKIP_INST*1] ...
 * ...
 * [START_INST+MAX_INST*i+SKIP_INST*i ... END_INST] ...
 *
 * Initialization is set to trace nothing.
 */
static uint64_t START_INST = 0;
static uint64_t MAX_INST = 1;
static uint64_t SKIP_INST = 0;
static uint64_t END_INST = 0;

/********************************************************************
 * Parameters for TraceSpecifiedInterval mode.
 *******************************************************************/

/**
 * Intervals, which contains lines like lhs rhs.
 * Each line represent an interval of [lhs, rhs)
 */
static std::list<std::pair<uint64_t, uint64_t>> intervals;

static uint64_t PRINT_INTERVAL = 10000000;

} // namespace

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

static void initializeIntervals() {
  const char *fn = std::getenv("LLVM_TDG_INTERVALS_FILE");
  assert(fn != nullptr &&
         "Failed to find environment LLVM_TDG_INTERVALS_FILE.");
  std::ifstream in(fn);
  assert(in.is_open() && "Failed to open interval file.");
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    uint64_t lhs, rhs;
    if (!(iss >> lhs >> rhs)) {
      continue;
    }
    printf("Found interval [%lu, %lu).\n", lhs, rhs);
    assert(lhs < rhs && "Invalid interval.");
    if (!intervals.empty()) {
      assert(lhs >= intervals.back().second &&
             "Intervals should be specifid in increasing order.");
    }
    intervals.emplace_back(lhs, rhs);
  }
}

static void initializeTraceMode() {
  traceMode = static_cast<TraceMode>(getUint64Env("LLVM_TDG_TRACE_MODE"));
  printf("Initializing traceMode to %d.\n", static_cast<int>(traceMode));
  if (traceMode == TraceMode::TraceUniformInterval) {
    START_INST = getUint64Env("LLVM_TDG_START_INST");
    SKIP_INST = getUint64Env("LLVM_TDG_SKIP_INST");
    MAX_INST = getUint64Env("LLVM_TDG_MAX_INST");
    END_INST = getUint64Env("LLVM_TDG_END_INST");
  } else if (traceMode == TraceMode::TraceSpecifiedInterval) {
    initializeIntervals();
  }
}

static void initializeTraceROI() {
  traceROI = static_cast<TraceROI>(getUint64Env("LLVM_TDG_TRACE_ROI"));
}

static void cleanup();

static void initialize() {
  static bool initialized = false;
  if (!initialized) {
    printf("initializing tracer...\n");

    traceFolder = std::getenv("LLVM_TDG_TRACE_FOLDER");
    if (!traceFolder) {
      traceFolder = DEFAULT_TRACE_FOLDER;
    }

    // Create the instruction uid file.
    const char *instUIDMapFileName = std::getenv("LLVM_TDG_INST_UID_FILE");
    assert(instUIDMapFileName != nullptr &&
           "Please provide the inst uid file.");
    instUIDMap.parseFrom(instUIDMapFileName);

    initializeTraceMode();
    initializeTraceROI();

    PRINT_INTERVAL = getUint64Env("LLVM_TDG_PRINT_INTERVAL");
    if (PRINT_INTERVAL == 0) {
      PRINT_INTERVAL = 10000000;
    }

    // Set profile file name.

    isDebug = getBoolEnv("LLVM_TDG_DEBUG");

    std::atexit(cleanup);

    printf("Initializing traceFolder to %s.\n", traceFolder);
    printf("Initializing traceROI to %d.\n", static_cast<int>(traceROI));
    printf("Initializing SKIP_INST to %lu...\n", SKIP_INST);
    printf("Initializing MAX_INST to %lu...\n", MAX_INST);
    printf("Initializing START_INST to %lu...\n", START_INST);
    printf("Initializing END_INST to %lu...\n", END_INST);
    printf("Initializing PRINT_INTERVAL to %lu...\n", PRINT_INTERVAL);
    printf("Initializing isDebug to %d...\n", isDebug);
    initialized = true;
  }
}

void checkIsMain(const char *functionName) {
  if (hasSeenMain) {
    return;
  }
  const char *mainFunctionName = "main";
  while ((*functionName) != 0 && (*mainFunctionName) != 0) {
    if ((*functionName) != (*mainFunctionName)) {
      return;
    }
    ++functionName;
    ++mainFunctionName;
  }
  if ((*functionName) == 0 && (*mainFunctionName) == 0) {
    hasSeenMain = true;
  }
}

// Extension for multi-thread.
struct TracerThreadState {
public:
  bool insideMyself = false;
  // ! TracerThreadState initialization should be protected by lock.
  bool initialized = false;
  pthread_t tid;
  int seqTid = 0;
  uint64_t count = 0;
  uint64_t countInTraceFunc = 0;
  uint64_t tracedCount = 0;
  uint64_t hardExitCount = 1e10;
  const LLVM::TDG::InstructionDescriptor *currentInstDescriptor = nullptr;
  size_t currentInstValueDescriptorId = 0;

  // Profile the code.
  ProfileLogger profile;

  /**
   * The tracer will maintain a stack at run time to determine if we are in a
   * specified function region.
   */
  std::vector<unsigned> stack;
  std::vector<const char *> stackName;
  unsigned tracedFunctionsInStack = 0;
  const char *currentInstOpName = nullptr;

  // Thread-local storage, used to pass in values larger than register, e.g.
  // vector.
  static constexpr size_t MaxTLSBytes = 4096;
  uint8_t tlsBuffer[MaxTLSBytes];

  size_t samples = 0;
  char fileName[1024];
  static constexpr size_t MaxProfileFileNameBytes = 128;
  char profileFileName[MaxProfileFileNameBytes];

  const char *getNewTraceFileName();
  void getProfileFileName(char *buf, size_t size);
  bool isInROI();
  bool shouldLog();
  bool shouldSwitchTraceFile();
  bool shouldExit();
  void printStack();
  void pushStack(const char *funcName, bool isTraced);
  void popStack();
  void unwind();
};

TracerThreadState threadStates[MaxNThreads];
pthread_mutex_t threadStatesAllocationLock;

static void cleanup() {
  static constexpr size_t profileFileNameSize = 128;
  static char profileFileName[profileFileNameSize];
  for (auto i = 0; i < MaxNThreads; ++i) {
    auto &tts = threadStates[i];
    if (tts.initialized) {
      tts.getProfileFileName(profileFileName, profileFileNameSize);
      std::string profileFileNameStr(profileFileName);
      tts.profile.serializeToFile(profileFileNameStr);
    }
  }
  /**
   * FIX IT!!
   * We really should deallocate this one, but it will cause
   * segment fault in the destructor and I have no idea how to fix it.
   * Since the memory will be recollected by the OS anyway, I leave it here.
   */
  printf("Done!\n");
}

TracerThreadState &getOrInitializeThreadState() {
  /**
   * ! This function is not protected by insideMyself.
   * ! Must contain no code that may trigger recursive call chain.
   * ! Also it is called before initialize(). So anything initialized in
   * ! initialize() should not be used.
   */
  auto tid = pthread_self();
  for (size_t i = 0; i < MaxNThreads; ++i) {
    auto &tts = threadStates[i];
    if (tts.initialized) {
      if (pthread_equal(tts.tid, tid) != 0) {
        return tts;
      } else {
        continue;
      }
    } else {
      // ! We have to acquire the lock to allocate new states.
      pthread_mutex_lock(&threadStatesAllocationLock);
      if (!tts.initialized) {
        // We got this one.
        tts.tid = tid;
        tts.initialized = true;
        tts.seqTid = i;
        // Initialize the hard exit count.
        {
          auto hardExitInMillion =
              getUint64Env("LLVM_TDG_HARD_EXIT_IN_MILLION");
          if (hardExitInMillion > 0) {
            tts.hardExitCount = hardExitInMillion * 1e6;
          }
          printf("initialize hardExitCount to %lu.\n", tts.hardExitCount);
        }

        // Initialize the profiler.
        auto profileIntervalSize =
            getUint64Env("LLVM_TDG_PROFILE_INTERVAL_SIZE");
        if (profileIntervalSize == 0) {
          profileIntervalSize = 10000000;
        }
        tts.profile.initialize(profileIntervalSize);
        pthread_mutex_unlock(&threadStatesAllocationLock);
        return tts;
      } else {
        // This one has been occupied by another thread.
        pthread_mutex_unlock(&threadStatesAllocationLock);
        continue;
      }
    }
  }
  printf("Reach the maximum number of threads.\n");
  std::exit(1);
}

const char *getNewTraceFileName(int tid) {
  auto &tts = threadStates[tid];
  assert(tts.initialized && "This thread state should be initialized.");
  return tts.getNewTraceFileName();
}

const char *TracerThreadState::getNewTraceFileName() {
  std::sprintf(fileName, "%s/%d.%zu.trace", traceFolder, seqTid, samples);
  samples++;
  return fileName;
}

void TracerThreadState::getProfileFileName(char *buf, size_t size) {
  auto writtenBytes =
      std::snprintf(buf, size, "%s/%d.profile", traceFolder, seqTid);
  if (writtenBytes >= 0 && writtenBytes < size) {
    return;
  }
  printf("Failed to get profile file name.\n");
  std::exit(1);
}

/**
 * This function decides if we are in ROI.
 */
bool TracerThreadState::isInROI() {
  switch (traceROI) {
  case TraceROI::All: {
    return true;
  }
  case TraceROI::SpecifiedFunction: {
    return tracedFunctionsInStack > 0;
  }
  default: { assert(false && "Illegal traceROI."); }
  }
}

/**
 * This functions decides if it's time to trace.
 */
bool TracerThreadState::shouldLog() {

  // First we check if we are actually in the ROI.
  if (!isInROI()) {
    return false;
  }

  // Get the correct instruction count;
  auto instCount = count;
  if (traceROI == TraceROI::SpecifiedFunction) {
    instCount = countInTraceFunc;
  }

  switch (traceMode) {
  case TraceMode::Profile: {
    return false;
  }
  case TraceMode::TraceAll: {
    return true;
  }
  case TraceMode::TraceUniformInterval: {
    if (instCount >= START_INST) {
      return (instCount - START_INST) % (MAX_INST + SKIP_INST) < MAX_INST;
    } else {
      return false;
    }
  }
  case TraceMode::TraceSpecifiedInterval: {
    if (intervals.empty())
      return false;
    const auto &interval = intervals.front();
    if (instCount < interval.first) {
      return false;
    }
    return instCount < interval.second;
  }
  default: { assert(false && "Unknown trace mode."); }
  }
}

bool TracerThreadState::shouldSwitchTraceFile() {
  // Get the correct instruction count;
  auto instCount = count;
  if (traceROI == TraceROI::SpecifiedFunction) {
    instCount = countInTraceFunc;
  }
  switch (traceMode) {
  case TraceMode::Profile:
  case TraceMode::TraceAll: {
    return false;
  }
  case TraceMode::TraceUniformInterval: {
    if ((instCount - START_INST) % (MAX_INST + SKIP_INST) == (MAX_INST - 1)) {
      // We are the last one of every MAX_INST. Switch file.
      return true;
    } else {
      return false;
    }
  }
  case TraceMode::TraceSpecifiedInterval: {
    if (intervals.empty()) {
      return false;
    }
    if (instCount == intervals.front().second - 1) {
      printf("Finish tracing interval [%lu, %lu).\n", intervals.front().first,
             intervals.front().second);
      intervals.pop_front();
      return true;
    }
    return false;
  }
  default: { assert(false && "Unknown trace mode."); }
  }
}

bool TracerThreadState::shouldExit() {
  // Hard exit condition.
  if (count == this->hardExitCount) {
    std::exit(0);
  }

  switch (traceMode) {
  case TraceMode::Profile:
  case TraceMode::TraceAll: {
    return false;
  }
  case TraceMode::TraceUniformInterval: {
    if (traceROI == TraceROI::SpecifiedFunction) {
      return countInTraceFunc == END_INST;
    } else {
      return count == END_INST;
    }
  }
  case TraceMode::TraceSpecifiedInterval: {
    return intervals.empty();
  }
  default: { assert(false && "Unknown trace mode."); };
  }
}

void TracerThreadState::printStack() {
  for (size_t i = 0; i < stackName.size(); ++i) {
    printf("%s (%u) -> ", stackName[i], stack[i]);
  }
  printf("\n");
}

void TracerThreadState::pushStack(const char *funcName, bool isTraced) {
  stack.push_back(isTraced);
  stackName.push_back(funcName);
  if (isTraced) {
    tracedFunctionsInStack++;
  }
}

void TracerThreadState::popStack() {
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

// Try to use libunwind to unwind the stack to some point. Return if
// succeed.
void TracerThreadState::unwind() {
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
  // For every frame in our stack, search in the unwinded stack, if there
  // is no match, then we assume this is the boundary.
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

#define TracerLibEnter()                                                       \
  if (!hasSeenMain) {                                                          \
    return;                                                                    \
  }                                                                            \
  auto &tts = getOrInitializeThreadState();                                    \
  if (tts.insideMyself) {                                                      \
    return;                                                                    \
  } else {                                                                     \
    tts.insideMyself = true;                                                   \
  }

#define TracerLibExit()                                                        \
  {                                                                            \
    tts.insideMyself = false;                                                  \
    return;                                                                    \
  }

uint8_t *getOrAllocateDataBuffer(uint64_t size) {
  /**
   * So far we take a simple approach: a statically allocated 1 page for each
   * thread.
   * ! No need to protect this function insideMyself because it is impossible to
   * ! get recursive call chain.
   */
  auto &tts = getOrInitializeThreadState();
  assert(size <= TracerThreadState::MaxTLSBytes &&
         "Exceed the maximum allowed size for data buffer.");
  return tts.tlsBuffer;
}

void printFuncEnter(const char *FunctionName, unsigned IsTraced) {
  // Main function should always be traced.
  checkIsMain(FunctionName);
  TracerLibEnter();
  initialize();
  // printf("%s:%d, inside? %d\n", __FILE__, __LINE__, insideMyself);
  // Update the stack only if we considered specified function as ROI.
  if (traceROI == TraceROI::SpecifiedFunction) {
    tts.pushStack(FunctionName, IsTraced);
  }
  // printf("Entering %s, stack depth %lu\n", FunctionName, stack.size());
  if (tts.shouldLog()) {
    printFuncEnterImpl(tts.seqTid, FunctionName);
  }
  TracerLibExit();
}

void printInst(const char *FunctionName, uint64_t UID) {
  TracerLibEnter();

  tts.currentInstDescriptor = &(instUIDMap.getDescriptor(UID));
  tts.currentInstValueDescriptorId = 0;
  auto OpCodeName = tts.currentInstDescriptor->op().c_str();
  auto BBName = tts.currentInstDescriptor->bb().c_str();
  // auto FunctionName = instDescriptor.FuncName.c_str();
  auto Id = tts.currentInstDescriptor->pos_in_bb();

  // Check if this is a landingpad instruction if we want to have the stack.
  if (traceROI == TraceROI::SpecifiedFunction) {
    if (std::strcmp(OpCodeName, "landingpad") == 0) {
      // This is a landingpad instruction.
      // Use libunwind to adjust our stack.
      if (isDebug) {
        printf("Before unwind the stack: \n");
        tts.printStack();
      }
      tts.unwind();
      assert(!tts.stackName.empty() && "After unwind the stack is empty.");

      if (isDebug) {
        printf("After unwind the stack: \n");
        tts.printStack();
      }
    }
    if (tts.stackName.back() != FunctionName) {
      printf("Unmatched FunctionName %s %s %u with stack: \n", FunctionName,
             BBName, Id);
      tts.printStack();
      std::exit(1);
    }
  }

  std::string FuncStr(FunctionName);
  std::string BBStr(BBName);
  /**
   * Profile if we are in ROI.
   * Normally we should profile for every basic block,
   * but it is similar to profile for every dynamic instruction,
   * and the profiler needs to know the instruction count to
   * correctly partition the profiling interval.
   */
  if (tts.isInROI()) {
    tts.profile.addBasicBlock(FuncStr, BBStr);
  }

  // printf("%s %s:%d, inside? %d count %lu\n", FunctionName, __FILE__,
  // __LINE__, insideMyself, count); Update current inst.
  assert(tts.currentInstOpName == nullptr &&
         "Previous inst has not been closed.");
  tts.currentInstOpName = OpCodeName;
  tts.count++;
  if (traceROI == TraceROI::SpecifiedFunction &&
      tts.tracedFunctionsInStack > 0) {
    tts.countInTraceFunc++;
  }
  if (tts.count % PRINT_INTERVAL < 5) {
    // Print every PRINT_INTERVAL instructions.
    printf("[%d]%lu:", tts.seqTid, tts.count);
    for (size_t i = 0; i < tts.stackName.size(); ++i) {
      printf("%s (%u) -> ", tts.stackName[i], tts.stack[i]);
    }
    printf(" %s %u %s\n", BBName, Id, OpCodeName);
    if (tts.count % PRINT_INTERVAL == 4) {
      printf("\n");
    }
  }
  if (tts.shouldLog()) {
    printInstImpl(tts.seqTid, FunctionName, BBName, Id, UID, OpCodeName);
  }
  TracerLibExit();
}

void printValue(const char Tag, const char *Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  TracerLibEnter();
  if (!tts.shouldLog()) {
    TracerLibExit();
  }

  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
  case TypeID::LabelTyID: {
    // For label, log the name again to be compatible with other type.
    printValueLabelImpl(tts.seqTid, Tag, Name, TypeId);
    break;
  }
  case TypeID::IntegerTyID: {
    uint64_t value = va_arg(VAList, uint64_t);
    printValueIntImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  // Float is promoted to double on x64.
  case TypeID::FloatTyID:
  case TypeID::DoubleTyID: {
    double value = va_arg(VAList, double);
    printValueFloatImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  case TypeID::PointerTyID: {
    void *value = va_arg(VAList, void *);
    printValuePointerImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  case TypeID::VectorTyID: {
    uint32_t size = va_arg(VAList, uint32_t);
    uint8_t *buffer = va_arg(VAList, uint8_t *);
    printValueVectorImpl(tts.seqTid, Tag, Name, TypeId, size, buffer);
    break;
  }
  default: {
    printValueUnsupportImpl(tts.seqTid, Tag, Name, TypeId);
    break;
  }
  }
  va_end(VAList);
  TracerLibExit();
}

void printInstValue(unsigned NumAdditionalArgs, ...) {
  TracerLibEnter();
  if (!tts.shouldLog()) {
    TracerLibExit();
  }

  assert(tts.currentInstDescriptor != nullptr && "Missing inst descriptor.");
  assert(tts.currentInstValueDescriptorId <
             tts.currentInstDescriptor->values_size() &&
         "Overflow of value descriptor id.");

  const auto &currentInstValueDescriptor =
      tts.currentInstDescriptor->values(tts.currentInstValueDescriptorId);
  tts.currentInstValueDescriptorId++;

  auto TypeId = currentInstValueDescriptor.type_id();
  auto Tag = currentInstValueDescriptor.is_param() ? PRINT_VALUE_TAG_PARAMETER
                                                   : PRINT_VALUE_TAG_RESULT;
  // TODO: Totally remove Name.
  const char *Name = "";

  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
  case TypeID::LabelTyID: {
    // For label, log the name again to be compatible with other type.
    // assert(false && "So far printInstValue does not work for label type.");
    const char *labelName = va_arg(VAList, const char *);
    printValueLabelImpl(tts.seqTid, Tag, labelName, TypeId);
    break;
  }
  case TypeID::IntegerTyID: {
    uint64_t value = va_arg(VAList, uint64_t);
    printValueIntImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  // Float is promoted to double on x64.
  case TypeID::FloatTyID:
  case TypeID::DoubleTyID: {
    double value = va_arg(VAList, double);
    printValueFloatImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  case TypeID::PointerTyID: {
    void *value = va_arg(VAList, void *);
    printValuePointerImpl(tts.seqTid, Tag, Name, TypeId, value);
    break;
  }
  case TypeID::VectorTyID: {
    uint32_t size = va_arg(VAList, uint32_t);
    uint8_t *buffer = va_arg(VAList, uint8_t *);
    printValueVectorImpl(tts.seqTid, Tag, Name, TypeId, size, buffer);
    break;
  }
  default: {
    printValueUnsupportImpl(tts.seqTid, Tag, Name, TypeId);
    break;
  }
  }
  va_end(VAList);
  TracerLibExit();
}

void printInstEnd() {
  TracerLibEnter();
  if (tts.shouldLog()) {
    tts.tracedCount++;
    printInstEndImpl(tts.seqTid);
    // Check if we are about to switch file.
    if (tts.shouldSwitchTraceFile()) {
      switchFileImpl(tts.seqTid);
    }
  }

  // Update the stack only when we try to measure in traced functions.
  if (traceROI == TraceROI::SpecifiedFunction) {
    assert(tts.currentInstOpName != nullptr &&
           "Missing currentInstOpName in printInstEnd.");
    if (std::strcmp(tts.currentInstOpName, "ret") == 0) {
      // printf("%s\n", currentInstOpName.c_str());
      if (isDebug) {
        printf("Return from: ");
        tts.printStack();
      }
      tts.popStack();
    }
  }

  tts.currentInstOpName = nullptr;

  // Check if we are about to exit.
  if (tts.shouldExit()) {
    std::exit(0);
  }

  TracerLibExit();
}

void printFuncEnterEnd() {
  TracerLibEnter();
  if (tts.shouldLog()) {
    printFuncEnterEndImpl(tts.seqTid);
  }
  TracerLibExit();
}
