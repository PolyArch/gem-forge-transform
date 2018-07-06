/**
 * This pass will apply SIMD transformation to the datagraph.
 */

#include "Replay.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <set>
#include <unordered_map>
#include <unordered_set>

static llvm::cl::opt<uint8_t>
    VectorizeWidth("simd-vec-width",
                   llvm::cl::desc("SIMD vectorization width"));
static llvm::cl::opt<float> VectorizeEfficiencyThreshold(
    "simd-vec-threshold",
    llvm::cl::desc("SIMD vectorization efficiency threshold"));

#define DEBUG_TYPE "SIMDPass"
namespace {

/**
 * Determine if the static inst is the head of a loop.
 */
bool isStaticInstLoopHead(llvm::Loop *Loop, llvm::Instruction *StaticInst) {
  assert(Loop != nullptr && "Null loop for isStaticInstLoopHead.");
  return (Loop->getHeader() == StaticInst->getParent()) &&
         (StaticInst->getParent()->getFirstNonPHI() == StaticInst);
}

std::string printLoop(llvm::Loop *Loop) {
  return std::string(Loop->getHeader()->getParent()->getName()) +
         "::" + std::string(Loop->getName());
}

/**
 * When ModulePass requires FunctionPass, each time the getAnalysis<>(F) will
 * rerun the analysis and override the previous result.
 * In order to improve performance, we cache some information of loop in this
 * class to reduce calls to getAnalysis<>(F).
 */
class StaticInnerMostLoop {
public:
  explicit StaticInnerMostLoop(llvm::Loop *Loop);
  StaticInnerMostLoop(const StaticInnerMostLoop &Other) = delete;
  StaticInnerMostLoop(StaticInnerMostLoop &&Other) = delete;
  StaticInnerMostLoop &operator=(const StaticInnerMostLoop &Other) = delete;
  StaticInnerMostLoop &operator=(StaticInnerMostLoop &&Other) = delete;

  /**
   * Return the first non-phi inst in the header.
   */
  llvm::Instruction *getHeaderNonPhiInst();

  llvm::BasicBlock *getHeader() const { return this->BBList.front(); }

  /**
   * Contains the basic blocks in topological sorted order, where the first
   * one is the header.
   */
  std::list<llvm::BasicBlock *> BBList;

  std::string print() const {
    return std::string(this->getHeader()->getParent()->getName()) +
           "::" + std::string(this->getHeader()->getName());
  }

  size_t StaticInstCount;

private:
  /**
   * Sort all the basic blocks in topological order and store the result in
   * BBList.
   */
  void scheduleBasicBlocksInLoop(llvm::Loop *Loop);
};

/**
 * This serves as a containter for some intermediate information specific to
 * a certain loop.
 */
class DynamicInnerMostLoop {
public:
  using DynamicId = DataGraph::DynamicId;

  /**
   * Construct the map from [_Begin, _End).
   */
  DynamicInnerMostLoop(
      StaticInnerMostLoop *_StaticLoop, uint8_t _LoopIter,
      const std::unordered_map<llvm::BasicBlock *, StaticInnerMostLoop *>
          &_BBToStaticLoopMap,
      DataGraph::DynamicInstIter _Begin, DataGraph::DynamicInstIter _End);
  DynamicInnerMostLoop(const DynamicInnerMostLoop &Other) = delete;
  DynamicInnerMostLoop(DynamicInnerMostLoop &&Other) = delete;
  DynamicInnerMostLoop &operator=(const DynamicInnerMostLoop &Other) = delete;
  DynamicInnerMostLoop &operator=(DynamicInnerMostLoop &&Other) = delete;

  void print(llvm::raw_ostream &OStream) const;

  bool contains(llvm::BasicBlock *BB) const;

  StaticInnerMostLoop *StaticLoop;
  const uint8_t LoopIter;

  /**
   * Data structure to map from static instructions to dynamic iteration
   * for inner most loops.
   * The Datagraph class doesn't maintain a map from static instructions
   * to dynamic instructions to simplify its implementation as well as
   * make it more general.
   * However, it is handy if we can have this information when we are
   * processing loops.
   * If there is some control flow in the loop and some static instructions
   * do not have dynamic instruction in some iteration, we simply put
   * DynamicInstruction::InvalidId there.
   */
  std::unordered_map<llvm::Instruction *, std::vector<DynamicId>>
      StaticToDynamicMap;

  /**
   * When we creating new vectorize instructions, we have to record the
   * map from static instructions to newly created dynamic ones.
   */
  std::unordered_map<llvm::Instruction *, DynamicId> StaticToNewDynamicMap;

  /**
   * Newly created dynamic instructions for this loop.
   */
  std::list<DynamicInstruction *> NewDynamicInstList;

private:
  /**
   * Contains the map from bb to static loop. Used to implement contains.
   */
  const std::unordered_map<llvm::BasicBlock *, StaticInnerMostLoop *>
      &BBToStaticLoopMap;
};

class SIMDPass : public ReplayTrace {
public:
  static char ID;
  SIMDPass()
      : ReplayTrace(ID), State(SEARCHING), VectorizeWidth(2),
        VectorizeEfficiencyThreshold(0.8f) {
    if (::VectorizeWidth.getNumOccurrences() > 0) {
      this->VectorizeWidth = ::VectorizeWidth.getValue();
    }
    if (::VectorizeEfficiencyThreshold.getNumOccurrences() > 0) {
      this->VectorizeEfficiencyThreshold =
          ::VectorizeEfficiencyThreshold.getValue();
    }
  }

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    ReplayTrace::getAnalysisUsage(Info);
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
  }

protected:
  bool initialize(llvm::Module &Module) override {
    // Reset memorization.
    this->BBToStaticLoopMap.clear();
    this->FunctionsWithCachedStaticLoop.clear();
    // Reset other variables.
    this->State = SEARCHING;
    // Call the base initialization.
    return ReplayTrace::initialize(Module);
  }

  bool finalize(llvm::Module &Module) override {

    // Release the cached static loops.
    std::unordered_set<StaticInnerMostLoop *> Released;
    for (const auto &Iter : this->BBToStaticLoopMap) {
      if (Released.find(Iter.second) == Released.end()) {
        Released.insert(Iter.second);
        DEBUG(llvm::errs() << "Releasing StaticInnerMostLoop at " << Iter.second
                           << '\n');
        delete Iter.second;
      }
    }
    this->BBToStaticLoopMap.clear();
    this->FunctionsWithCachedStaticLoop.clear();

    return ReplayTrace::finalize(Module);
  }

  enum {
    SEARCHING,
    BUFFERING,
  } State;

  uint8_t VectorizeWidth;
  float VectorizeEfficiencyThreshold;

  /**
   * This set contains functions we have already cached the StaticInnerMostLoop.
   */
  std::unordered_set<llvm::Function *> FunctionsWithCachedStaticLoop;

  /**
   * Maps the bb to its inner most loop. This speeds up how we
   * decide when we hitting the header of a loop in the trace stream.
   */
  std::unordered_map<llvm::BasicBlock *, StaticInnerMostLoop *>
      BBToStaticLoopMap;

  /**
   * Check if this function has already had cached static loop. If not, build
   * the HeaderNonPhiInstToStaticLoopMap.
   * This will only add those static vectorizable loops, determined by
   * isLoopStaticVectorizable().
   */
  void buildStaticLoopsIfNecessary(llvm::Function *Function);

  /**
   * Check if this basic block is in some candidate loop.
   * This will call buildStaticLoopsIfNecessary.
   * Returns nullptr if not.
   */
  StaticInnerMostLoop *getStaticLoopForBB(llvm::BasicBlock *BB);

  void transform() override;

  /**
   * Determine if a loop is vectorizable statically, with memorization.
   * This filters out some loops.
   * 1. Inner most loops.
   * 2. Only calls to some math functions.
   */
  bool isLoopStaticVectorizable(llvm::Loop *Loop);

  /**
   * Compute the loop efficiency for the buffered loop.
   * Efficency = DynamicInsts / LoopIter * StaticInsts.
   */
  float computeLoopEfficiency(DynamicInnerMostLoop &DynamicLoop);

  /**
   * Checks if there is memory dependency between iterations for the
   * buffered loop.
   */
  bool isLoopInterIterMemDependent(DynamicInnerMostLoop &DynamicLoop);

  /**
   * TODO: Give this function a better name.
   * Checks if the loop is data dependent on previous iteration.
   * We check every memory access in the loop.
   * 1. If the address can is an affine AddRec SCEV, we believe it's
   *    vectorizable.
   * 2. If the memory access is strided, we also mark it vectorizable.
   *    This means that although the compiler cannot statically determine
   *    the access pattern, it turns out to be an array access.
   * 3. Otherwise, things gets tricky. For now we just say it's not
   *    vectorizable.
   *    We can do a BFS on the register deps to see if it is dependent
   *    on some load insts in previous iterations.If so, then this is not
   *    vectorizable.
   *    For case 3, we also care about the control dependence of the
   *    memory access. Here we take an aggressive approach:
   *      If in the buffered iterations, there is no control dependent
   *      scattered memory accesses, we take it vectorizable.
   */
  bool isLoopInterIterDataDependent(DynamicInnerMostLoop &DynamicLoop);

  /**
   * Determine if a loop is vectorizable dynamically.
   * This can not be memorized as it requires dynamic information.
   * 1. Super block efficiency.
   * 2. No inter-iter memory dependency.
   * 3. No inter-iter data dependency.
   */
  bool isLoopDynamicVectorizable(DynamicInnerMostLoop &DynamicLoop);

  /**
   * Handle scatter (incontinuous) load instruction.
   * When the loaded address is not continous, we have to scalarize and
   * insert #vec_width loads and pack them into a vector.
   * @return the number of newly inserted dynamic instructions.
   */
  size_t createDynamicInstsForScatterLoad(llvm::LoadInst *StaticLoad,
                                          DynamicInnerMostLoop &DynamicLoop);

  /**
   * Handle scatter (incontinuous) store instruction.
   * When the stored address is not continous, we have to scalarize and
   * unpack and insert #vec_width stores.
   * @return the number of newly inserted dynamic instructions.
   */
  size_t createDynamicInstsForScatterStore(llvm::LoadInst *StaticStore,
                                           DynamicInnerMostLoop &DynamicLoop);

  /**
   * Create instructions for general cases, including continuous load/store.
   */
  size_t createDynamicInsts(llvm::Instruction *StaticInst,
                            DynamicInnerMostLoop &DynamicLoop);

  /**
   * Do the actual transformation on the loop.
   * TODO: Handle internal control flow. Currently we will just assert.
   */
  void simdTransform(DynamicInnerMostLoop &DynamicLoop);

  /**
   * Process the buffered instructions when the buffered iterations reaches
   * VectorizeWidth.
   * May perform actual vectorization or not dependent on the dynamic
   * information. Commit all the buffered instructions EXCEPT THE LAST ONE,
   * which either belongs to next iteration or other regions.
   */
  void processBuffer(StaticInnerMostLoop *StaticLoop, uint8_t LoopIter);
};

void SIMDPass::buildStaticLoopsIfNecessary(llvm::Function *Function) {
  if (this->FunctionsWithCachedStaticLoop.find(Function) !=
      this->FunctionsWithCachedStaticLoop.end()) {
    // This function has already been processed.
    return;
  }
  auto &LoopInfo =
      getAnalysis<llvm::LoopInfoWrapperPass>(*Function).getLoopInfo();
  for (auto Loop : LoopInfo.getLoopsInPreorder()) {
    if (this->isLoopStaticVectorizable(Loop)) {
      // We only cache the statically vectorizable loops.
      auto StaticLoop = new StaticInnerMostLoop(Loop);
      for (auto BB : StaticLoop->BBList) {
        auto Inserted = this->BBToStaticLoopMap.emplace(BB, StaticLoop).second;
        assert(Inserted && "The bb has already been inserted.");
      }
    }
  }
  this->FunctionsWithCachedStaticLoop.insert(Function);
}

StaticInnerMostLoop *SIMDPass::getStaticLoopForBB(llvm::BasicBlock *BB) {
  this->buildStaticLoopsIfNecessary(BB->getParent());
  auto Iter = this->BBToStaticLoopMap.find(BB);
  if (Iter == this->BBToStaticLoopMap.end()) {
    return nullptr;
  }
  return Iter->second;
}

void SIMDPass::transform() {
  assert(this->Trace != nullptr && "Must have a trace to be transformed.");

  /**
   * A state machine.
   * SEARCHING:
   *    Initial state. Searching for the start of an inner most loop
   * iteration. If encounter an inner most loop iteration, switch to
   * buffering. Otherwise, commit this instruction. BUFFERING: Buffer until
   * some vectorize factor iterations (default 4) for candidate for
   * transformation. Instructions will not be commited. Once done, check if
   * the buffered loop is vectorizable. If so, estimate the speed up. If
   *    everything looks fine, transform the datagraph. Otherwise, commit the
   *    buffered instructions.
   */
  this->State = SEARCHING;
  uint64_t Count = 0;

  // Varaibles valid only for BUFFERING state.
  StaticInnerMostLoop *CurrentStaticLoop;
  uint8_t LoopIter;

  DEBUG(llvm::errs() << "SIMD Pass start.\n");

  while (true) {

    auto NewDynamicInst = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    StaticInnerMostLoop *NewStaticLoop = nullptr;
    bool IsAtHeaderOfVectorizable = false;

    if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      NewStaticLoop = this->getStaticLoopForBB(NewStaticInst->getParent());
      if (NewStaticLoop) {
        IsAtHeaderOfVectorizable =
            NewStaticInst == NewStaticLoop->getHeaderNonPhiInst();
      }

      if (IsAtHeaderOfVectorizable) {
        DEBUG(llvm::errs() << "Hitting header of new candidate loop "
                           << NewStaticLoop->print() << "\n");
      }

    } else {
      // Commit any remaining buffered instructions when hitting the end.
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      break;
    }
    // State machine.
    switch (this->State) {
    case SEARCHING: {
      /**
       * Ideally this should commit immediately.
       * However, for a conditional branch inst, the current implementation
       * would require one more instruction in the future to determine the
       * next BB (so that we have ground truth for branch predictor).
       * This means that we better have at least two instructions in the
       * datagraph when committing one instruction (very hacky).
       * Make sure it is.
       * TODO: Fix this by adding an internal buffer inside datagraph so
       *    that user will not have to do this hack.
       */

      assert(this->Trace->DynamicInstructionList.size() <= 2 &&
             "For searching state, there should be at most 2 dynamic "
             "instructions in the buffer.");

      if (this->Trace->DynamicInstructionList.size() == 2) {
        // We can commit the previous one.
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }

      if (IsAtHeaderOfVectorizable) {
        this->State = BUFFERING;
        LoopIter = 0;
        CurrentStaticLoop = NewStaticLoop;
      }

      break;
    }
    case BUFFERING: {

      /**
       * Update the LoopIter.
       * 1. If we are inside other loops, increase iter count.
       * 2. If we are in the same loop, but hitting header, increase iter
       * count.
       */
      if (NewStaticLoop != CurrentStaticLoop) {
        // We are out of the current loop.
        LoopIter++;
      } else if (IsAtHeaderOfVectorizable) {
        LoopIter++;
      }

      /**
       * Process the buffer if LoopIter reaches vectorize width.
       * This will commit the buffer EXCEPT THE LAST INSTRUCTION.
       */
      if (LoopIter == this->VectorizeWidth) {
        // DEBUG(llvm::errs() << "SIMD Pass: Vectorizing.\n");
        this->processBuffer(CurrentStaticLoop, LoopIter);
        // Clear the loop iter.
        LoopIter = 0;
      }

      /**
       * Determine next state.
       * 1. If we are in the same loop, keep buffering.
       * 2. If we are in other loop,
       *  2.1 If we are at the header, and if the new loop is
       *      vectorizable. If so, keep buffering the new loop.
       *  2.2. Otherwise, go back to search.
       * In case 2, we should also commit remaining buffered iters
       * in case the number of total iters is not a multiple of vectorize
       * width.
       */
      if (NewStaticLoop != CurrentStaticLoop) {
        // Case 2
        // Commit remaining insts.
        while (this->Trace->DynamicInstructionList.size() > 1) {
          this->Serializer->serialize(
              this->Trace->DynamicInstructionList.front(), this->Trace);
          this->Trace->commitOneDynamicInst();
        }
        if (IsAtHeaderOfVectorizable) {
          // Case 2.1.
          // Update the loop and loop iter.
          LoopIter = 0;
          CurrentStaticLoop = NewStaticLoop;
        } else {
          // Case 2.2.
          // DEBUG(llvm::errs() << "SIMD Pass: Go back to searching
          // state.\n");
          this->State = SEARCHING;
        }
      }
      break;
    }
    default: {
      llvm_unreachable("SIMD Pass: Invalid machine state.");
      break;
    }
    }
  }

  DEBUG(llvm::errs() << "SIMD Pass end.\n");
}

bool SIMDPass::isLoopStaticVectorizable(llvm::Loop *Loop) {

  assert(Loop != nullptr && "Loop should not be nullptr.");

  if (!Loop->empty()) {
    // This is not the inner most loop.
    DEBUG(llvm::errs() << "SIMDPass: Loop " << printLoop(Loop)
                       << " is statically not vectorizable because it is not "
                          "inner most loop.\n");
    return false;
  }

  // Check if there is any calls to unsupported function.
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
      if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(&*InstIter)) {
        // This is a call inst.
        auto Callee = CallInst->getCalledFunction();
        if (Callee == nullptr) {
          // Indirect call, not vectorizable.
          DEBUG(llvm::errs() << "SIMDPass: Loop " << printLoop(Loop)
                             << " is statically not vectorizable because it "
                                "contains indirect call.\n");
          return false;
        }
        // Check if calling some supported math function.
        if (Callee->getName() != "sin") {
          // TODO: add a set for supported functions.
          DEBUG(llvm::errs() << "SIMDPass: Loop " << printLoop(Loop)
                             << " is statically not vectorizable because it "
                                "contains unsupported call.\n");
          return false;
        }
      }
    }
  }

  // Done: this loop is statically vectorizable.
  return true;
}

float SIMDPass::computeLoopEfficiency(DynamicInnerMostLoop &DynamicLoop) {

  size_t StaticInstCount = DynamicLoop.StaticLoop->StaticInstCount;

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");
  size_t DynamicInstCount = this->Trace->DynamicInstructionList.size() - 1;
  return static_cast<float>(DynamicInstCount) /
         static_cast<float>(StaticInstCount * DynamicLoop.LoopIter);
}

bool SIMDPass::isLoopInterIterMemDependent(DynamicInnerMostLoop &DynamicLoop) {

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");

  // Iterate through all the store/load instructions.
  for (auto BB : DynamicLoop.StaticLoop->BBList) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if ((!llvm::isa<llvm::StoreInst>(StaticInst)) &&
          (!llvm::isa<llvm::LoadInst>(StaticInst))) {
        continue;
      }

      // Check the memory dependence for all the dynamic instructions.
      auto &DynamicIds = DynamicLoop.StaticToDynamicMap.at(StaticInst);
      for (size_t CurrentIter = 0; CurrentIter < DynamicIds.size();
           ++CurrentIter) {
        auto Id = DynamicIds[CurrentIter];
        if (Id == DynamicInstruction::InvalidId) {
          continue;
        }
        auto MemDepsIter = this->Trace->MemDeps.find(Id);
        if (MemDepsIter == this->Trace->MemDeps.end()) {
          // No memory dependence.
          continue;
        }
        for (auto MemDepId : MemDepsIter->second) {
          /**
           * Two cases:
           * 1. If the dependent dynamic id is not alive, then it falls out of
           *    the current vectorization width, ignore it. This is very
           *    optimistic.
           * 2. If the dependent dynamic id is alive, check which iteration
           *    it is. If the same iteration, ignore it. Otherwise, this is
           *    an iter-iter memory dependence and we have to bail out.
           */
          auto AliveDynamicInstsMapIter =
              this->Trace->AliveDynamicInstsMap.find(MemDepId);
          if (AliveDynamicInstsMapIter ==
              this->Trace->AliveDynamicInstsMap.end()) {
            // Case 1. continue.
            continue;
          }

          // Case 2. Check if the dependent dynamic id is in the same
          // iteration.

          // If the dependent dynamic is alive, it must comes from the same
          // loop.
          auto MemDepStaticInst =
              (*(AliveDynamicInstsMapIter->second))->getStaticInstruction();
          assert(DynamicLoop.contains(MemDepStaticInst->getParent()) &&
                 "If alive, the memory dependent static instruction should be "
                 "in the same loop.");
          auto &MemDepDynamicIds =
              DynamicLoop.StaticToDynamicMap.at(MemDepStaticInst);
          if (MemDepDynamicIds[CurrentIter] == MemDepId) {
            // The dependent memory instruction is in the same loop. Ignore
            // it.
            continue;
          }

          // This is a memory dependence for previous iterations within the
          // vectorization width.

          DEBUG(llvm::errs() << "Iter mem dependence for iter " << CurrentIter
                             << " inst " << StaticInst->getName() << '\n');

          return true;
        }
      }
    }
  }

  return false;
}

bool SIMDPass::isLoopInterIterDataDependent(DynamicInnerMostLoop &DynamicLoop) {

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");

  auto Function = DynamicLoop.StaticLoop->getHeader()->getParent();
  auto &SE = getAnalysis<llvm::ScalarEvolutionWrapperPass>(*Function).getSE();

  // Iterate through all the store/load instructions.
  for (auto BB : DynamicLoop.StaticLoop->BBList) {

    DEBUG(llvm::errs() << 100 << DynamicLoop.StaticLoop->print() << '\n');
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if ((!llvm::isa<llvm::StoreInst>(StaticInst)) &&
          (!llvm::isa<llvm::LoadInst>(StaticInst))) {
        continue;
      }

      // Get the address of the memory access.
      // True for load.
      bool isLoadOrStore = llvm::isa<llvm::LoadInst>(StaticInst);
      unsigned AddrOperandIdx = isLoadOrStore ? 0 : 1;
      llvm::Value *StaticAddr = StaticInst->getOperand(AddrOperandIdx);

      // Case 1. SECV Affine AddRec.
      {
        const llvm::SCEV *SCEV = SE.getSCEV(StaticAddr);
        if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
          if (AddRecSCEV->isAffine()) {
            continue;
          }
        }
      }

      // Case 2. Check the dynamic stride.
      {
        auto &DynamicIds = DynamicLoop.StaticToDynamicMap.at(StaticInst);
        // Use INT64_MAX as an invalid value.
        int64_t Stride = INT64_MAX;
        for (size_t Iter = 1; Iter < DynamicIds.size(); ++Iter) {
          auto ThisIterDynamicId = DynamicIds[Iter];
          auto PrevIterDynamicId = DynamicIds[Iter - 1];
          int64_t NewStride = INT64_MAX;
          // If both are valid ids, we compute the new stride.
          if ((ThisIterDynamicId != DynamicInstruction::InvalidId) &&
              (PrevIterDynamicId != DynamicInstruction::InvalidId)) {
            DynamicInstruction *ThisIterDynamicInst =
                *(this->Trace->AliveDynamicInstsMap.at(ThisIterDynamicId));
            DynamicInstruction *PrevIterDynamicInst =
                *(this->Trace->AliveDynamicInstsMap.at(PrevIterDynamicId));
            uint64_t ThisIterDynamicAddr = std::stoul(
                ThisIterDynamicInst->DynamicOperands[AddrOperandIdx]->Value);
            uint64_t PrevIterDynamicAddr = std::stoul(
                PrevIterDynamicInst->DynamicOperands[AddrOperandIdx]->Value);
            NewStride = ThisIterDynamicAddr - PrevIterDynamicAddr;
          }
          if (NewStride != Stride) {
            // Failed computing stride. Reset Stride and break.
            Stride = INT64_MAX;
            break;
          }
        }
        // Check if we have found a valid stride.
        if (Stride != INT64_MAX) {
          continue;
        }
      }

      // Case 3. Non-strided memory access. Simply return true for now.

      return true;
    }
  }

  DEBUG(llvm::errs() << 200 << DynamicLoop.StaticLoop->print() << '\n');

  return false;
}

bool SIMDPass::isLoopDynamicVectorizable(DynamicInnerMostLoop &DynamicLoop) {

  DEBUG(llvm::errs() << 1 << DynamicLoop.StaticLoop->print() << '\n');
  float Efficiency = this->computeLoopEfficiency(DynamicLoop);
  if (Efficiency < this->VectorizeEfficiencyThreshold) {
    DEBUG(llvm::errs() << "Failed due to inefficiency\n");
    return false;
  }

  DEBUG(llvm::errs() << 2 << DynamicLoop.StaticLoop->print() << '\n');
  if (this->isLoopInterIterMemDependent(DynamicLoop)) {
    DEBUG(llvm::errs() << "Failed due to inter-iter memory dependent\n");
    return false;
  }

  DEBUG(llvm::errs() << 3 << DynamicLoop.StaticLoop->print() << '\n');
  if (this->isLoopInterIterDataDependent(DynamicLoop)) {
    DEBUG(llvm::errs() << "Failed due to inter-iter data dependent\n");
    return false;
  }
  DEBUG(llvm::errs() << 4 << DynamicLoop.StaticLoop->print() << '\n');

  return true;
}

void SIMDPass::simdTransform(DynamicInnerMostLoop &DynamicLoop) {

  /**
   * This works like a abstract interpreter which walks the
   * inner most loop and insert simd version of dynamic instructions
   * into the datagraph.
   * Finally it clears the buffered dynamic instructions with the
   * newly created simd ones.
   */

  /**
   * First we need to topological sort the basic blocks if we want to
   * handle internal control flow in the future.
   */
  DEBUG(llvm::errs() << DynamicLoop.StaticLoop->print() << '\n');
  const auto &Schedule = DynamicLoop.StaticLoop->BBList;

  DEBUG(llvm::errs() << "Creating new simd instructions.\n");

  for (auto BB : Schedule) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      llvm::Instruction *StaticInst = &*InstIter;
      if (llvm::isa<llvm::PHINode>(StaticInst)) {
        // Ignore all phi node.
        continue;
      }

      // For now, simply all the basic case.
      this->createDynamicInsts(StaticInst, DynamicLoop);
    }
  }

  /**
   * Fix the dependence for the last dynamic instruction, which does not
   * belongs to this simd transformation. If we can peek the trace stream, we
   * don't have to deal with this annoying +1 thing.
   */
  {

    DEBUG(llvm::errs()
          << "Fixing the dependence for the last outside instruction.\n");
    // Lambda does not support capture const reference directly.
    const auto &ConstDynamicLoop = DynamicLoop;
    auto FixDependence =
        [this, &ConstDynamicLoop](
            std::unordered_set<DynamicInstruction::DynamicId> &Deps) -> void {
      std::unordered_set<DynamicInstruction::DynamicId> NewDeps;
      DEBUG(llvm::errs() << "Deps size " << Deps.size() << '\n');
      for (auto DepIter = Deps.begin(); DepIter != Deps.end();) {
        auto DepId = *DepIter;
        auto DepDynamicInstIter = this->Trace->AliveDynamicInstsMap.find(DepId);
        if (DepDynamicInstIter == this->Trace->AliveDynamicInstsMap.end()) {
          // This is outside loop dependence, keep it.
          ++DepIter;
          DEBUG(llvm::errs() << "Keep dependence " << DepId
                             << " for last outside instruction.\n");
          continue;
        }
        // The dependent inst is simd transformed.
        auto DepStaticInst =
            (*(DepDynamicInstIter->second))->getStaticInstruction();
        const auto DepNewDynamicInstIter =
            ConstDynamicLoop.StaticToNewDynamicMap.find(DepStaticInst);
        assert(DepNewDynamicInstIter !=
                   ConstDynamicLoop.StaticToNewDynamicMap.end() &&
               "Missing simd transformed dependent instruction.");
        DEBUG(llvm::errs() << "Replacing dependence " << DepId << " with "
                           << DepNewDynamicInstIter->second << '\n');
        NewDeps.insert(DepNewDynamicInstIter->second);
        DepIter = Deps.erase(DepIter);
      }
      // Insert the new dependence.
      Deps.insert(NewDeps.begin(), NewDeps.end());
    };
    FixDependence(this->Trace->RegDeps.at(
        this->Trace->DynamicInstructionList.back()->Id));
    FixDependence(this->Trace->MemDeps.at(
        this->Trace->DynamicInstructionList.back()->Id));
    FixDependence(this->Trace->CtrDeps.at(
        this->Trace->DynamicInstructionList.back()->Id));
  }

  // Commit (without serialize to output) all the buffered instructions
  // (except the last one).
  DEBUG(llvm::errs() << "SIMD Pass: Original trace.===========\n");
  while (this->Trace->DynamicInstructionList.size() > 1) {
    DEBUG(this->Trace->DynamicInstructionList.front()->format(llvm::errs(),
                                                              this->Trace));
    this->Trace->commitOneDynamicInst();
  }
  DEBUG(llvm::errs() << "SIMD Pass: Original trace: End.======\n");

  /**
   * Officially add the new dynamic instruction into the datagraph.
   * "Officially" as the dependence is already stored in DG->Reg/Mem/CtrDeps.
   * 1. Insert into the dynamic instruction list.
   * 2. Insert into the alive map.
   * 3. Update the StaticToLastDynamicMap.
   */
  auto InsertionPoint = this->Trace->DynamicInstructionList.end();
  --InsertionPoint;
  for (auto NewDynamicInst : DynamicLoop.NewDynamicInstList) {
    auto NewDynamicInstIter = this->Trace->DynamicInstructionList.insert(
        InsertionPoint, NewDynamicInst);
    this->Trace->AliveDynamicInstsMap.emplace(NewDynamicInst->Id,
                                              NewDynamicInstIter);
    this->Trace
        ->StaticToLastDynamicMap[NewDynamicInst->getStaticInstruction()] =
        NewDynamicInst->Id;
  }

  DEBUG(DynamicLoop.print(llvm::errs()));

  // Commit all the newly added instructions (except the last one from next
  // iter).
  DEBUG(llvm::errs() << "SIMD Pass: Serializing.===============\n");
  while (this->Trace->DynamicInstructionList.size() > 1) {
    DEBUG(this->Trace->DynamicInstructionList.front()->format(llvm::errs(),
                                                              this->Trace));
    this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                this->Trace);
    this->Trace->commitOneDynamicInst();
  }
  DEBUG(llvm::errs() << "SIMD Pass: Serializing: Done.=========\n");
}

/**
 * Represent a simd store with a widen size and value.
 * This is a very hacky way, we just hijack the serialized protobuf message
 * and modify it.
 */
class SIMDStoreInst : public LLVMDynamicInstruction {
public:
  SIMDStoreInst(llvm::Instruction *_StaticInstruction, DynamicValue *_Result,
                std::vector<DynamicValue *> _Operands,
                const std::string &_VecData)
      : LLVMDynamicInstruction(_StaticInstruction, _Result,
                               std::move(_Operands)),
        VecData(_VecData) {}
  // void formatCustomizedFields(std::ofstream &Out,
  //                             DataGraph *Trace) const override {
  //   assert(false && "Customized Fields for simd is not implemented.");
  // }

  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    LLVMDynamicInstruction::serializeToProtobufExtra(ProtobufEntry, DG);
    assert(ProtobufEntry->has_store() && "StoreExtra not set.");
    auto StoreExtra = ProtobufEntry->mutable_store();
    // Set the actual stored value.
    StoreExtra->set_size(this->VecData.size());
    StoreExtra->set_value(this->VecData);
  }

  const std::string VecData;
};

class SIMDLoadInst : public LLVMDynamicInstruction {
public:
  SIMDLoadInst(llvm::Instruction *_StaticInstruction, DynamicValue *_Result,
               std::vector<DynamicValue *> _Operands, const size_t &_VecSize)
      : LLVMDynamicInstruction(_StaticInstruction, _Result,
                               std::move(_Operands)),
        VecSize(_VecSize) {}
  // void formatCustomizedFields(std::ofstream &Out,
  //                             DataGraph *Trace) const override {
  //   assert(false && "Customized Fields for simd is not implemented.");
  // }

  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    LLVMDynamicInstruction::serializeToProtobufExtra(ProtobufEntry, DG);
    assert(ProtobufEntry->has_load() && "LoadExtra not set.");
    // Set the actual load size.
    auto LoadExtra = ProtobufEntry->mutable_load();
    LoadExtra->set_size(this->VecSize * LoadExtra->size());
  }

  const size_t VecSize;
};

size_t
SIMDPass::createDynamicInstsForScatterLoad(llvm::LoadInst *StaticLoad,
                                           DynamicInnerMostLoop &DynamicLoop) {
  return 0;
}
size_t
SIMDPass::createDynamicInstsForScatterStore(llvm::LoadInst *StaticStore,
                                            DynamicInnerMostLoop &DynamicLoop) {
  return 0;
}

size_t SIMDPass::createDynamicInsts(llvm::Instruction *StaticInst,
                                    DynamicInnerMostLoop &DynamicLoop) {

  const auto &DynamicIds = DynamicLoop.StaticToDynamicMap.at(StaticInst);

  // First copy the dynamic values.
  std::vector<DynamicValue *> DynamicOperands;
  DynamicValue *DynamicResult = nullptr;
  {
    assert(DynamicIds[0] != DynamicInstruction::InvalidId &&
           "Missing first dynamic instruction.");
    auto FirstDynamicInst = this->Trace->AliveDynamicInstsMap.at(DynamicIds[0]);
    for (auto DynamicOperand : (*FirstDynamicInst)->DynamicOperands) {
      DynamicOperands.push_back(new DynamicValue(*DynamicOperand));
    }
    auto OldResult = (*FirstDynamicInst)->DynamicResult;
    if (OldResult != nullptr) {
      DynamicResult = new DynamicValue(*OldResult);
    }
  }

  DynamicInstruction *NewDynamicInst = nullptr;

  // Create the new dynamic instruction.
  // For load and store, we will update the AddrToLastLoad/StoreMap
  if (auto StaticLoad = llvm::dyn_cast<llvm::LoadInst>(StaticInst)) {
    // Continuous load.
    NewDynamicInst =
        new SIMDLoadInst(StaticInst, DynamicResult, std::move(DynamicOperands),
                         DynamicLoop.LoopIter);
    size_t Addr =
        std::stoul(NewDynamicInst->DynamicOperands[0]->Value, nullptr, 16);
    llvm::Type *LoadedType =
        StaticLoad->getPointerOperandType()->getPointerElementType();
    uint64_t LoadedTypeSizeInByte =
        this->Trace->DataLayout->getTypeStoreSize(LoadedType);
    for (size_t I = 0; I < DynamicLoop.LoopIter * LoadedTypeSizeInByte; ++I) {
      this->Trace->updateAddrToLastMemoryAccessMap(Addr + I, NewDynamicInst->Id,
                                                   true /* true for load*/);
    }

  } else if (auto StaticStore = llvm::dyn_cast<llvm::StoreInst>(StaticInst)) {
    // Continuous store, simply use the first store's address.
    // Pack the store data.
    std::string StoredData;
    auto Type = StaticStore->getPointerOperandType()->getPointerElementType();
    for (auto DynamicId : DynamicIds) {
      assert(DynamicId != DynamicInstruction::InvalidId && "Missing store.");
      auto DynamicStoreIter = this->Trace->AliveDynamicInstsMap.at(DynamicId);
      // Concatenate the store value for all stores.
      StoredData +=
          (*DynamicStoreIter)->DynamicOperands[0]->serializeToBytes(Type);
    }

    NewDynamicInst = new SIMDStoreInst(StaticInst, DynamicResult,
                                       std::move(DynamicOperands), StoredData);

    size_t Addr =
        std::stoul(NewDynamicInst->DynamicOperands[1]->Value, nullptr, 16);
    for (size_t I = 0; I < StoredData.size(); ++I) {
      this->Trace->updateAddrToLastMemoryAccessMap(Addr + I, NewDynamicInst->Id,
                                                   false /*false for store*/);
    }

  } else {
    // Normal instruction.
    NewDynamicInst = new LLVMDynamicInstruction(StaticInst, DynamicResult,
                                                std::move(DynamicOperands));
  }

  // DEBUG(llvm::errs() << "Created simd instruction for " <<
  // StaticInst->getName()
  //                    << "=" << StaticInst->getOpcodeName() << " id "
  //                    << NewDynamicInst->Id << '\n');

  // Time to fix the dependence.
  // DEBUG(llvm::errs() << "Fixing dependence for new simd instruction.\n");
  auto &NewRegDeps = this->Trace->RegDeps
                         .emplace(std::piecewise_construct,
                                  std::forward_as_tuple(NewDynamicInst->Id),
                                  std::forward_as_tuple())
                         .first->second;
  auto &NewMemDeps = this->Trace->MemDeps
                         .emplace(std::piecewise_construct,
                                  std::forward_as_tuple(NewDynamicInst->Id),
                                  std::forward_as_tuple())
                         .first->second;
  auto &NewCtrDeps = this->Trace->CtrDeps
                         .emplace(std::piecewise_construct,
                                  std::forward_as_tuple(NewDynamicInst->Id),
                                  std::forward_as_tuple())
                         .first->second;

  for (auto DynamicId : DynamicIds) {
    if (DynamicId == DynamicInstruction::InvalidId) {
      continue;
    }
    // We transfer the dependence from outside these iterations directly into
    // the new set.
    auto TransferDepIds =
        [this](const std::unordered_set<DynamicInstruction::DynamicId> &Deps,
               std::unordered_set<DynamicInstruction::DynamicId> &NewDeps)
        -> void {
      for (auto DepId : Deps) {
        // If the dependent id is not alive, it is from outside the loop.
        // We should probabily handle this more elegantly.
        if (this->Trace->AliveDynamicInstsMap.find(DepId) ==
            this->Trace->AliveDynamicInstsMap.end()) {
          // Outside dependence, transfer directly.
          NewDeps.insert(DepId);
        }
      }
    };
    // DEBUG(llvm::errs() << "Transferring outside loop dependence.\n");
    TransferDepIds(this->Trace->RegDeps.at(DynamicId), NewRegDeps);
    TransferDepIds(this->Trace->MemDeps.at(DynamicId), NewMemDeps);
    TransferDepIds(this->Trace->CtrDeps.at(DynamicId), NewCtrDeps);

    // Fix the memory dependence within the loop.
    // DEBUG(llvm::errs() << "Fixing inside loop memory dependence.\n");
    for (auto MemDepId : this->Trace->MemDeps.at(DynamicId)) {
      auto MemDepDynamicInstIter =
          this->Trace->AliveDynamicInstsMap.find(MemDepId);
      if (MemDepDynamicInstIter == this->Trace->AliveDynamicInstsMap.end()) {
        continue;
      }
      auto MemDepStaticInst =
          (*(MemDepDynamicInstIter->second))->getStaticInstruction();

      auto NewMemDepDynamicInstIter =
          DynamicLoop.StaticToNewDynamicMap.find(MemDepStaticInst);
      assert(NewMemDepDynamicInstIter !=
                 DynamicLoop.StaticToNewDynamicMap.end() &&
             "Missing new dynamic simd instruction for memory dependence.");
      NewMemDeps.insert(NewMemDepDynamicInstIter->second);
    }
  }

  // We fix the register dependence within the loop.
  // DEBUG(llvm::errs() << "Fixing the register dependence within the
  // loop.\n");
  for (unsigned OperandId = 0, NumOperands = StaticInst->getNumOperands();
       OperandId != NumOperands; ++OperandId) {
    auto Operand = StaticInst->getOperand(OperandId);
    if (auto OpInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
      // There is register dependence.
      if (!DynamicLoop.contains(OpInst->getParent())) {
        // Outside loop register dependence.
        continue;
      }
      auto NewOpDynamicInstIter =
          DynamicLoop.StaticToNewDynamicMap.find(OpInst);
      if (NewOpDynamicInstIter != DynamicLoop.StaticToNewDynamicMap.end()) {
        NewRegDeps.insert(NewOpDynamicInstIter->second);
      } else {
        // Otherwise, it should be dependent on some un transformed phi node,
        // which means it is actually an induction variable and should be
        // actually outside dependence. Here I just check it's a phi node.
        assert(llvm::isa<llvm::PHINode>(OpInst) &&
               "Missing new simd instrunction for non-phi node.");
      }
    }
  }

  // Add this dynamic instruction into DynamicLoop.
  DynamicLoop.StaticToNewDynamicMap.emplace(StaticInst, NewDynamicInst->Id);
  DynamicLoop.NewDynamicInstList.push_back(NewDynamicInst);

  return 1;
}

void SIMDPass::processBuffer(StaticInnerMostLoop *StaticLoop,
                             uint8_t LoopIter) {

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");

  // Construct the internal data struct.
  auto Begin = this->Trace->DynamicInstructionList.begin();
  auto End = this->Trace->DynamicInstructionList.end();
  --End;
  DynamicInnerMostLoop DynamicLoop(StaticLoop, LoopIter,
                                   this->BBToStaticLoopMap, Begin, End);

  DEBUG(llvm::errs() << DynamicLoop.StaticLoop->print() << '\n');

  if (!this->isLoopDynamicVectorizable(DynamicLoop)) {
    // This loop is not dynamically vectorizable, simply commit it.
    DEBUG(llvm::errs() << "SIMD Pass: Loop is not dynamically vectorizable.\n");
    DEBUG(DynamicLoop.print(llvm::errs()));
    while (this->Trace->DynamicInstructionList.size() > 1) {
      this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                  this->Trace);
      this->Trace->commitOneDynamicInst();
    }
    return;
  }

  DEBUG(llvm::errs() << DynamicLoop.StaticLoop->print() << '\n');
  DEBUG(llvm::errs() << "SIMD Pass: Transforming Loop.===========\n");
  this->simdTransform(DynamicLoop);
  DEBUG(llvm::errs() << "SIMD Pass: Transforming Loop: Done.=====\n");
}

StaticInnerMostLoop::StaticInnerMostLoop(llvm::Loop *Loop)
    : StaticInstCount(0) {
  assert(Loop->empty() && "Should be inner most loops.");
  this->scheduleBasicBlocksInLoop(Loop);
  assert(!this->BBList.empty() && "Empty loops.");

  for (auto BB : this->BBList) {
    auto PHIs = BB->phis();
    this->StaticInstCount +=
        BB->size() - std::distance(PHIs.begin(), PHIs.end());
  }
}

llvm::Instruction *StaticInnerMostLoop::getHeaderNonPhiInst() {
  if (this->BBList.empty()) {
    return nullptr;
  }
  return this->BBList.front()->getFirstNonPHI();
}

void StaticInnerMostLoop::scheduleBasicBlocksInLoop(llvm::Loop *Loop) {
  assert(Loop != nullptr && "Null Loop for scheduleBasicBlocksInLoop.");

  DEBUG(llvm::errs() << "Schedule basic blocks in loop " << printLoop(Loop)
                     << '\n');

  auto &Schedule = this->BBList;

  std::list<std::pair<llvm::BasicBlock *, bool>> Stack;
  std::unordered_set<llvm::BasicBlock *> Scheduled;

  Stack.emplace_back(Loop->getHeader(), false);
  while (!Stack.empty()) {
    auto &Entry = Stack.back();
    auto BB = Entry.first;
    if (Scheduled.find(BB) != Scheduled.end()) {
      // This BB has already been scheduled.
      Stack.pop_back();
      continue;
    }
    if (Entry.second) {
      // This is the second time we visit BB.
      // We can schedule it.
      Schedule.push_front(BB);
      Scheduled.insert(BB);
      Stack.pop_back();
      continue;
    }
    // This is the first time we visit BB, do DFS.
    Entry.second = true;
    for (auto Succ : llvm::successors(BB)) {
      if (!Loop->contains(Succ)) {
        continue;
      }
      if (Succ == BB) {
        // Ignore myself.
        continue;
      }
      Stack.emplace_back(Succ, false);
    }
  }

  return;
}

DynamicInnerMostLoop::DynamicInnerMostLoop(
    StaticInnerMostLoop *_StaticLoop, uint8_t _LoopIter,
    const std::unordered_map<llvm::BasicBlock *, StaticInnerMostLoop *>
        &_BBToStaticLoopMap,
    DataGraph::DynamicInstIter _Begin, DataGraph::DynamicInstIter _End)
    : StaticLoop(_StaticLoop), LoopIter(_LoopIter),
      BBToStaticLoopMap(_BBToStaticLoopMap) {

  assert(this->StaticLoop != nullptr && "DynamicInnerMostLoop: Null loop.");

  /**
   * Create the map at the beginning. This will ensure that we have the vector
   * for all the static instructions inside the loop.
   */
  for (auto BB : this->StaticLoop->BBList) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      assert(this->StaticToDynamicMap.find(StaticInst) ==
                 this->StaticToDynamicMap.end() &&
             "This static inst is already handled.");

      // First time. Construct a vector with size of loop iter and elements
      // of invalid id.
      this->StaticToDynamicMap.emplace(
          std::piecewise_construct, std::forward_as_tuple(StaticInst),
          std::forward_as_tuple(this->LoopIter, DynamicInstruction::InvalidId));
    }
  }

  /**
   * Actually the CurrentIter will range from [1, LoopIter].
   */
  uint8_t CurrentIter = 0;
  for (auto Iter = _Begin; Iter != _End; ++Iter) {

    auto StaticInst = (*Iter)->getStaticInstruction();
    assert(StaticInst != nullptr && "The static inst should not be null.");

    if (StaticInst == this->StaticLoop->getHeaderNonPhiInst()) {
      // We are at a new iteration.
      CurrentIter++;
    }

    assert(CurrentIter <= LoopIter &&
           "CurrentIter should not be greater than LoopIter.");
    assert(CurrentIter > 0 &&
           "We are not hitting the head of the loop at _Begin.");

    assert(this->StaticToDynamicMap.find(StaticInst) !=
               this->StaticToDynamicMap.end() &&
           "The static inst is not initialized in the map.");

    auto &DynamicIdVec = this->StaticToDynamicMap.at(StaticInst);
    assert(DynamicIdVec.size() == this->LoopIter &&
           "Invalid size of vector of dynamic id.");
    assert(DynamicIdVec[CurrentIter - 1] == DynamicInstruction::InvalidId &&
           "This slot of the vector is already initialized.");

    // Finally insert into the vector.
    DynamicIdVec[CurrentIter - 1] = (*Iter)->Id;
  }
}

void DynamicInnerMostLoop::print(llvm::raw_ostream &OStream) const {
  OStream << "StaticLoop: " << this->StaticLoop->print()
          << " Iter: " << static_cast<int>(this->LoopIter) << '\n';
}

bool DynamicInnerMostLoop::contains(llvm::BasicBlock *BB) const {
  auto Iter = this->BBToStaticLoopMap.find(BB);
  return Iter != this->BBToStaticLoopMap.end() &&
         Iter->second == this->StaticLoop;
}

} // namespace

#undef DEBUG_TYPE

char SIMDPass::ID = 0;
static llvm::RegisterPass<SIMDPass> X("simd-pass", "SIMD transform pass", false,
                                      false);
