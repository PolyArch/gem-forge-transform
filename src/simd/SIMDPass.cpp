/**
 * This pass will apply SIMD transformation to the datagraph.
 */

#include "LoopUtils.h"
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
 * This serves as a containter for some intermediate information specific to
 * a certain loop.
 */
class DynamicInnerMostLoop {
public:
  using DynamicId = DataGraph::DynamicId;

  /**
   * Construct the map from [_Begin, _End).
   */
  DynamicInnerMostLoop(StaticInnerMostLoop *_StaticLoop, uint8_t _LoopIter,
                       DataGraph *_DG, DataGraph::DynamicInstIter _Begin,
                       DataGraph::DynamicInstIter _End);
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
   * Data structure holding the computed stride for load/store.
   * If there is no constant stride, or this is a conditional load/store,
   * use INT64_MAX as an invalid value.
   * NOTICE: Only query this loop when you are sure it is a load/store.
   */
  std::unordered_map<llvm::Instruction *, int64_t> StaticToStrideMap;

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
   * Pointer to the datagraph.
   */
  DataGraph *DG;

  /**
   * Compute and fill in the StaticToStrideMap.
   */
  void computeStride(llvm::Instruction *StaticInst);
};

class SIMDPass : public ReplayTrace {
public:
  static char ID;
  SIMDPass()
      : ReplayTrace(ID), State(SEARCHING), VectorizeWidth(4),
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
    this->CachedLI = new CachedLoopInfo();
    // Reset other variables.
    this->State = SEARCHING;
    // Reset the StaticInnerMostLoop cache.
    this->CachedStaticInnerMostLoop.clear();
    // Call the base initialization.
    return ReplayTrace::initialize(Module);
  }

  bool finalize(llvm::Module &Module) override {
    // Remember to release the static inner most loop first.
    for (auto &Entry : this->CachedStaticInnerMostLoop) {
      delete Entry.second;
    }
    this->CachedStaticInnerMostLoop.clear();
    // Release the cached static loops.
    delete this->CachedLI;
    this->CachedLI = nullptr;
    return ReplayTrace::finalize(Module);
  }

  enum {
    SEARCHING,
    BUFFERING,
  } State;

  uint8_t VectorizeWidth;
  float VectorizeEfficiencyThreshold;

  /**
   * This class contains the cached loop info.
   */
  CachedLoopInfo *CachedLI;

  /**
   * Contains caches StaticInnerMostLoop.
   */
  std::unordered_map<llvm::Loop *, StaticInnerMostLoop *>
      CachedStaticInnerMostLoop;

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
   * Helper function to get the load/store type size.
   */
  uint64_t getTypeSizeForLoadStore(llvm::Instruction *StaticInst) const;

  /**
   * Helper function to transfer outside iteration register dependence from
   * OldDeps to NewDeps.
   */
  void
  transferOutsideRegDeps(DynamicInnerMostLoop &DynamicLoop,
                         DataGraph::RegDependenceList &NewDeps,
                         const DataGraph::RegDependenceList &OldDeps) const;

  /**
   * Helper function to transfer outside iteration register dependence from
   * OldDeps to NewDeps.
   */
  void transferOutsideNonRegDeps(DynamicInnerMostLoop &DynamicLoop,
                                 DataGraph::DependenceSet &NewDeps,
                                 const DataGraph::DependenceSet &OldDeps) const;

  /**
   * Helper function to transfer inside memory dependence.
   */
  void transferInsideMemDeps(DynamicInnerMostLoop &DynamicLoop,
                             DataGraph::DependenceSet &NewMemDeps,
                             const DataGraph::DependenceSet &OldMemDeps) const;

  /**
   * Helper function to transfer inside register dependence.
   */
  void transferInsideRegDeps(DynamicInnerMostLoop &DynamicLoop,
                             DataGraph::RegDependenceList &NewRegDeps,
                             llvm::Instruction *NewStaticInst) const;

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

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      auto NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr && this->isLoopStaticVectorizable(NewLoop)) {
        // This loop is vectorizable.
        if (this->CachedStaticInnerMostLoop.find(NewLoop) ==
            this->CachedStaticInnerMostLoop.end()) {
          this->CachedStaticInnerMostLoop.emplace(
              NewLoop, new StaticInnerMostLoop(NewLoop));
        }
        NewStaticLoop = this->CachedStaticInnerMostLoop.at(NewLoop);
        IsAtHeaderOfVectorizable = isStaticInstLoopHead(NewLoop, NewStaticInst);
      }

      if (IsAtHeaderOfVectorizable) {
        DEBUG(llvm::errs() << "Hitting header of new candidate loop "
                           << printLoop(NewLoop) << "\n");
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
           * Three cases:
           * 1. If the dependent dynamic id is not alive, then it falls out of
           *    the current vectorization width, ignore it. This is very
           *    optimistic.
           * 2. If the dependent dynamic id is alive, check which iteration
           *    it is. If the same iteration, ignore it. Otherwise, this is
           *    an inter-iter memory dependence.
           * 3. Even for inter-iter memory dependence, we also check the memory
           *    access order for a relaxed constraint.
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
          // Check the memory access order.
          auto ThisMemAccessOrder =
              DynamicLoop.StaticLoop->LoadStoreOrderMap.at(StaticInst);
          auto DepMemAccessOrder =
              DynamicLoop.StaticLoop->LoadStoreOrderMap.at(MemDepStaticInst);
          if (ThisMemAccessOrder > DepMemAccessOrder) {
            // The order is fine.
            continue;
          }

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
        int64_t Stride = DynamicLoop.StaticToStrideMap.at(StaticInst);
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

      // Check for scattered load/store.
      bool IsScatteredMemoryAccess = false;
      if (llvm::isa<llvm::LoadInst>(StaticInst) ||
          llvm::isa<llvm::StoreInst>(StaticInst)) {
        auto TypeSize = this->getTypeSizeForLoadStore(StaticInst);
        auto Stride = DynamicLoop.StaticToStrideMap.at(StaticInst);
        if (std::abs(Stride) > TypeSize) {
          IsScatteredMemoryAccess = true;
        }
      }

      if (IsScatteredMemoryAccess) {
        if (auto StaticLoad = llvm::dyn_cast<llvm::LoadInst>(StaticInst)) {
          this->createDynamicInstsForScatterLoad(StaticLoad, DynamicLoop);
        } else {
          assert(false && "Can not handle scattered store for now.");
        }
      } else {
        // Basic case.
        this->createDynamicInsts(StaticInst, DynamicLoop);
      }
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
    auto FixRegDependence =
        [this, &ConstDynamicLoop](DataGraph::RegDependenceList &Deps) -> void {
      for (auto &RegDep : Deps) {
        auto DepId = RegDep.second;
        auto DepDynamicInstIter = this->Trace->AliveDynamicInstsMap.find(DepId);
        if (DepDynamicInstIter == this->Trace->AliveDynamicInstsMap.end()) {
          // This is outside loop dependence, keep it.
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
        RegDep.second = DepNewDynamicInstIter->second;
      }
    };
    FixRegDependence(this->Trace->RegDeps.at(
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
   * 4. Update the DynamicFrame::PrevControlInstId.
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
    this->Trace->DynamicFrameStack.front().updatePrevControlInstId(
        NewDynamicInst);
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
    DEBUG(llvm::errs() << "Load addr " << LoadExtra->addr() << '\n');
    LoadExtra->set_size(this->VecSize * LoadExtra->size());
  }

  const size_t VecSize;
};

/**
 * Although this is really not an llvm instructions, we store the actual llvm
 * instruction it packs, to not break down the code when we insert it into the
 * datagraph. This should be handled more gracefully.
 */
class SIMDPackInst : public DynamicInstruction {
public:
  SIMDPackInst(llvm::Instruction *_StaticInst) : StaticInst(_StaticInst) {}
  std::string getOpName() const override { return "pack"; }
  llvm::Instruction *getStaticInstruction() override {
    return this->StaticInst;
  }
  llvm::Instruction *StaticInst;
  // No customized fields for pack.
};

uint64_t
SIMDPass::getTypeSizeForLoadStore(llvm::Instruction *StaticInst) const {

  assert(
      (llvm::isa<llvm::LoadInst>(StaticInst) ||
       llvm::isa<llvm::StoreInst>(StaticInst)) &&
      "getTypeSizeForLoadStore can not handle non-memory access instruction.");

  unsigned PointerOperandIdx = 0;
  if (llvm::isa<llvm::StoreInst>(StaticInst)) {
    PointerOperandIdx = 1;
  }

  llvm::Type *Type = StaticInst->getOperand(PointerOperandIdx)
                         ->getType()
                         ->getPointerElementType();
  return this->Trace->DataLayout->getTypeStoreSize(Type);
}

void SIMDPass::transferOutsideRegDeps(
    DynamicInnerMostLoop &DynamicLoop, DataGraph::RegDependenceList &NewDeps,
    const DataGraph::RegDependenceList &OldDeps) const {
  for (const auto &RegDep : OldDeps) {
    // If the dependent id is not alive, it is from outside the loop.
    // We should probabily handle this more elegantly.
    auto DepId = RegDep.second;
    if (this->Trace->getAliveDynamicInst(DepId) == nullptr) {
      // Outside dependence, transfer directly.
      NewDeps.emplace_back(RegDep.first, DepId);
    }
  }
}

void SIMDPass::transferOutsideNonRegDeps(
    DynamicInnerMostLoop &DynamicLoop, DataGraph::DependenceSet &NewDeps,
    const DataGraph::DependenceSet &OldDeps) const {
  for (const auto &DepId : OldDeps) {
    // If the dependent id is not alive, it is from outside the loop.
    // We should probabily handle this more elegantly.
    if (this->Trace->getAliveDynamicInst(DepId) == nullptr) {
      // Outside dependence, transfer directly.
      NewDeps.insert(DepId);
    }
  }
}

void SIMDPass::transferInsideMemDeps(
    DynamicInnerMostLoop &DynamicLoop, DataGraph::DependenceSet &NewMemDeps,
    const DataGraph::DependenceSet &OldMemDeps) const {
  for (auto MemDepId : OldMemDeps) {
    auto MemDepDynamicInst = this->Trace->getAliveDynamicInst(MemDepId);
    if (MemDepDynamicInst == nullptr) {
      // Outside memory dependence, ignore it.
      continue;
    }
    auto MemDepStaticInst = MemDepDynamicInst->getStaticInstruction();
    auto NewMemDepDynamicInstIter =
        DynamicLoop.StaticToNewDynamicMap.find(MemDepStaticInst);
    assert(NewMemDepDynamicInstIter !=
               DynamicLoop.StaticToNewDynamicMap.end() &&
           "Missing new dynamic simd instruction for memory dependence.");
    NewMemDeps.insert(NewMemDepDynamicInstIter->second);
  }
}

void SIMDPass::transferInsideRegDeps(DynamicInnerMostLoop &DynamicLoop,
                                     DataGraph::RegDependenceList &NewRegDeps,
                                     llvm::Instruction *NewStaticInst) const {
  for (unsigned OperandId = 0, NumOperands = NewStaticInst->getNumOperands();
       OperandId != NumOperands; ++OperandId) {
    auto Operand = NewStaticInst->getOperand(OperandId);
    if (auto OpInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
      // There is register dependence.
      if (!DynamicLoop.contains(OpInst->getParent())) {
        // Outside loop register dependence.
        continue;
      }
      auto NewOpDynamicInstIter =
          DynamicLoop.StaticToNewDynamicMap.find(OpInst);
      if (NewOpDynamicInstIter != DynamicLoop.StaticToNewDynamicMap.end()) {
        NewRegDeps.emplace_back(OpInst, NewOpDynamicInstIter->second);
      } else {
        // Otherwise, it should be dependent on some un transformed phi node,
        // which means it is actually an induction variable and should be
        // actually outside dependence. Here I just check it's a phi node.
        assert(llvm::isa<llvm::PHINode>(OpInst) &&
               "Missing new simd instrunction for non-phi node.");
      }
    }
  }
}

size_t
SIMDPass::createDynamicInstsForScatterLoad(llvm::LoadInst *StaticLoad,
                                           DynamicInnerMostLoop &DynamicLoop) {
  const auto &DynamicIds = DynamicLoop.StaticToDynamicMap.at(StaticLoad);
  std::vector<DynamicInstruction::DynamicId> NewDynamicIds;

  size_t CreatedInstCount = 0;
  // Scalarize the instruction.
  for (auto DynamicId : DynamicIds) {
    assert(DynamicId != LLVMDynamicInstruction::InvalidId &&
           "Conditional memory access found.");
    auto DynamicInst = this->Trace->getAliveDynamicInst(DynamicId);
    // Copy all the operands and result.
    std::vector<DynamicValue *> NewDynamicOperands;
    for (auto DynamicOperand : DynamicInst->DynamicOperands) {
      NewDynamicOperands.push_back(new DynamicValue(*DynamicOperand));
    }
    DynamicValue *NewDynamicResult = nullptr;
    if (DynamicInst->DynamicResult != nullptr) {
      NewDynamicResult = new DynamicValue(*(DynamicInst->DynamicResult));
    }
    auto NewDynamicInst = new LLVMDynamicInstruction(
        StaticLoad, NewDynamicResult, std::move(NewDynamicOperands));

    // Remember to fix the AddrToLastMemoryAccess map in datagraph.
    auto Addr = DynamicInst->DynamicOperands[0]->getAddr();
    uint64_t LoadedTypeSizeInByte = this->getTypeSizeForLoadStore(StaticLoad);
    for (size_t I = 0; I < LoadedTypeSizeInByte; ++I) {
      this->Trace->updateAddrToLastMemoryAccessMap(Addr + I, NewDynamicInst->Id,
                                                   true /* true for load*/);
    }

    // Fix the dependence.
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
    this->transferOutsideRegDeps(DynamicLoop, NewRegDeps,
                                 this->Trace->RegDeps.at(DynamicId));
    this->transferOutsideNonRegDeps(DynamicLoop, NewMemDeps,
                                    this->Trace->MemDeps.at(DynamicId));
    // For outside control dependence, only the first iteration has the currect
    // control dependence.
    this->transferOutsideNonRegDeps(DynamicLoop, NewCtrDeps,
                                    this->Trace->CtrDeps.at(DynamicIds[0]));

    this->transferInsideMemDeps(DynamicLoop, NewMemDeps,
                                this->Trace->MemDeps.at(DynamicId));
    this->transferInsideRegDeps(DynamicLoop, NewRegDeps, StaticLoad);

    NewDynamicIds.push_back(NewDynamicInst->Id);
    DynamicLoop.NewDynamicInstList.push_back(NewDynamicInst);
    ++CreatedInstCount;
  }

  // Create a fake pack instruction for these loads.
  // NOTE: although there is no memory dependence for pack, we have to create
  // the set to maintain the property that no dependence <-> empty set.
  auto NewDynamicPackInst = new SIMDPackInst(StaticLoad);
  auto &NewPackRegDeps =
      this->Trace->RegDeps
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple(NewDynamicPackInst->Id),
                   std::forward_as_tuple())
          .first->second;
  auto &NewPackMemDeps =
      this->Trace->MemDeps
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple(NewDynamicPackInst->Id),
                   std::forward_as_tuple())
          .first->second;
  auto &NewPackCtrDeps =
      this->Trace->CtrDeps
          .emplace(std::piecewise_construct,
                   std::forward_as_tuple(NewDynamicPackInst->Id),
                   std::forward_as_tuple())
          .first->second;
  for (auto NewDynamicId : NewDynamicIds) {
    // Make this pack dependent on all the loads.
    NewPackRegDeps.emplace_back(StaticLoad, NewDynamicId);
    // Copy all the loads control dependence to the pack.
    auto &NewLoadCtrDeps = this->Trace->CtrDeps.at(NewDynamicId);
    NewPackCtrDeps.insert(NewLoadCtrDeps.begin(), NewLoadCtrDeps.end());
  }

  // Make the static instruction map to the pack instruction, so that all the
  // user of this load will dependent on this pack.
  DynamicLoop.StaticToNewDynamicMap.emplace(StaticLoad, NewDynamicPackInst->Id);
  DynamicLoop.NewDynamicInstList.push_back(NewDynamicPackInst);
  ++CreatedInstCount;

  return CreatedInstCount;
}
size_t
SIMDPass::createDynamicInstsForScatterStore(llvm::LoadInst *StaticStore,
                                            DynamicInnerMostLoop &DynamicLoop) {
  assert(false && "Scattered store is not supported yet.");
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

  /**
   * Helper function to choose the base for continuous load/store.
   * If increasing, take the first one, otherwise the last one.
   */
  auto PickBaseForLoadStore =
      [this,
       &DynamicLoop](llvm::Instruction *StaticInst) -> DynamicInstruction * {
    assert((llvm::isa<llvm::LoadInst>(StaticInst) ||
            llvm::isa<llvm::StoreInst>(StaticInst)) &&
           "Trying to compute simd addr for non load/store instruction.");

    auto StrideMapIter = DynamicLoop.StaticToStrideMap.find(StaticInst);
    assert(StrideMapIter != DynamicLoop.StaticToStrideMap.end() &&
           "Missing stride in the map.");
    auto Stride = StrideMapIter->second;
    auto PickedId = DynamicInstruction::InvalidId;
    if (Stride > 0) {
      // Increasing address. Pick the first one.
      PickedId = DynamicLoop.StaticToDynamicMap.at(StaticInst).front();
    } else {
      // Decreasing address. Pick the last one.
      PickedId = DynamicLoop.StaticToDynamicMap.at(StaticInst).back();
    }
    DynamicInstruction *PickedInst = this->Trace->getAliveDynamicInst(PickedId);
    return PickedInst;
  };

  // Create the new dynamic instruction.
  // For load and store, we will update the AddrToLastLoad/StoreMap
  if (auto StaticLoad = llvm::dyn_cast<llvm::LoadInst>(StaticInst)) {
    // Continuous load.
    auto PickedInst = PickBaseForLoadStore(StaticInst);
    // Copy the new addr.
    *DynamicOperands[0] = *PickedInst->DynamicOperands[0];

    NewDynamicInst =
        new SIMDLoadInst(StaticInst, DynamicResult, std::move(DynamicOperands),
                         DynamicLoop.LoopIter);
    size_t Addr = NewDynamicInst->DynamicOperands[0]->getAddr();
    uint64_t LoadedTypeSizeInByte = this->getTypeSizeForLoadStore(StaticLoad);
    for (size_t I = 0; I < DynamicLoop.LoopIter * LoadedTypeSizeInByte; ++I) {
      this->Trace->updateAddrToLastMemoryAccessMap(Addr + I, NewDynamicInst->Id,
                                                   true /* true for load*/);
    }
  } else if (auto StaticStore = llvm::dyn_cast<llvm::StoreInst>(StaticInst)) {
    // Continuous store, simply use the first store's address.
    // Copy the new addr.
    auto PickedInst = PickBaseForLoadStore(StaticInst);
    // Copy the new addr.
    *DynamicOperands[1] = *PickedInst->DynamicOperands[1];

    // Pack the store data.
    std::string StoredData;
    auto Type = StaticStore->getPointerOperandType()->getPointerElementType();
    auto Stride = DynamicLoop.StaticToStrideMap.at(StaticInst);
    for (auto DynamicId : DynamicIds) {
      assert(DynamicId != DynamicInstruction::InvalidId && "Missing store.");
      auto DynamicStoreIter = this->Trace->AliveDynamicInstsMap.at(DynamicId);
      // Concatenate the store value for all stores.
      // Take care for negative stride.
      if (Stride > 0) {
        StoredData +=
            (*DynamicStoreIter)->DynamicOperands[0]->serializeToBytes(Type);
      } else {
        // Reverse concatenate.
        StoredData =
            (*DynamicStoreIter)->DynamicOperands[0]->serializeToBytes(Type) +
            StoredData;
      }
    }

    NewDynamicInst = new SIMDStoreInst(StaticInst, DynamicResult,
                                       std::move(DynamicOperands), StoredData);

    size_t Addr = NewDynamicInst->DynamicOperands[1]->getAddr();
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
    // DEBUG(llvm::errs() << "Transferring outside loop dependence.\n");
    this->transferOutsideRegDeps(DynamicLoop, NewRegDeps,
                                 this->Trace->RegDeps.at(DynamicId));
    this->transferOutsideNonRegDeps(DynamicLoop, NewMemDeps,
                                    this->Trace->MemDeps.at(DynamicId));
    this->transferOutsideNonRegDeps(DynamicLoop, NewCtrDeps,
                                    this->Trace->CtrDeps.at(DynamicId));

    // Fix the memory dependence within the loop.
    // DEBUG(llvm::errs() << "Fixing inside loop memory dependence.\n");
    this->transferInsideMemDeps(DynamicLoop, NewMemDeps,
                                this->Trace->MemDeps.at(DynamicId));
  }

  // We fix the register dependence within the loop.
  // DEBUG(llvm::errs() << "Fixing the register dependence within the
  // loop.\n");
  this->transferInsideRegDeps(DynamicLoop, NewRegDeps, StaticInst);

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
  DynamicInnerMostLoop DynamicLoop(StaticLoop, LoopIter, this->Trace, Begin,
                                   End);

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

DynamicInnerMostLoop::DynamicInnerMostLoop(StaticInnerMostLoop *_StaticLoop,
                                           uint8_t _LoopIter, DataGraph *_DG,
                                           DataGraph::DynamicInstIter _Begin,
                                           DataGraph::DynamicInstIter _End)
    : StaticLoop(_StaticLoop), LoopIter(_LoopIter), DG(_DG) {

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

  // Compute the stride for load/store.
  for (auto BB : this->StaticLoop->BBList) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if (llvm::isa<llvm::LoadInst>(StaticInst) ||
          llvm::isa<llvm::StoreInst>(StaticInst)) {
        this->computeStride(StaticInst);
      }
    }
  }
}

void DynamicInnerMostLoop::computeStride(llvm::Instruction *StaticInst) {
  assert((llvm::isa<llvm::LoadInst>(StaticInst) ||
          llvm::isa<llvm::StoreInst>(StaticInst)) &&
         "Trying to compute stride for non load/store instruction.");

  size_t AddrOperandIdx = 0;
  if (llvm::isa<llvm::StoreInst>(StaticInst)) {
    AddrOperandIdx = 1;
  }

  auto &DynamicIds = this->StaticToDynamicMap.at(StaticInst);
  // Use INT64_MAX as an invalid value.
  int64_t Stride = INT64_MAX;
  bool Initialized = false;
  for (size_t Iter = 1; Iter < DynamicIds.size(); ++Iter) {
    auto ThisIterDynamicId = DynamicIds[Iter];
    auto PrevIterDynamicId = DynamicIds[Iter - 1];
    int64_t NewStride = INT64_MAX;
    // If both are valid ids, we compute the new stride.
    if ((ThisIterDynamicId != DynamicInstruction::InvalidId) &&
        (PrevIterDynamicId != DynamicInstruction::InvalidId)) {
      DynamicInstruction *ThisIterDynamicInst =
          *(this->DG->AliveDynamicInstsMap.at(ThisIterDynamicId));
      DynamicInstruction *PrevIterDynamicInst =
          *(this->DG->AliveDynamicInstsMap.at(PrevIterDynamicId));
      uint64_t ThisIterDynamicAddr =
          ThisIterDynamicInst->DynamicOperands[AddrOperandIdx]->getAddr();
      uint64_t PrevIterDynamicAddr =
          PrevIterDynamicInst->DynamicOperands[AddrOperandIdx]->getAddr();
      NewStride = ThisIterDynamicAddr - PrevIterDynamicAddr;
    }
    if (!Initialized) {
      Stride = NewStride;
      Initialized = true;
    } else if (NewStride != Stride) {
      // Failed computing stride. Reset Stride and break.
      Stride = INT64_MAX;
      break;
    }
  }
  bool Inserted = this->StaticToStrideMap.emplace(StaticInst, Stride).second;
  assert(Inserted && "There is already a stride for this instruction.");
}

void DynamicInnerMostLoop::print(llvm::raw_ostream &OStream) const {
  OStream << "StaticLoop: " << this->StaticLoop->print()
          << " Iter: " << static_cast<int>(this->LoopIter) << '\n';
}

bool DynamicInnerMostLoop::contains(llvm::BasicBlock *BB) const {
  return this->StaticLoop->Loop->contains(BB);
}

} // namespace

#undef DEBUG_TYPE

char SIMDPass::ID = 0;
static llvm::RegisterPass<SIMDPass> X("simd-pass", "SIMD transform pass", false,
                                      false);
