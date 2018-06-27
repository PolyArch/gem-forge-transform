/**
 * This pass will apply SIMD transformation to the datagraph.
 */

#include "Replay.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
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
 * nullptr there.
 */
class DynamicInnerMostLoop {
public:
  using DynamicId = DataGraph::DynamicId;

  /**
   * Construct the map from [_Begin, _End).
   */
  DynamicInnerMostLoop(llvm::Loop *_Loop, uint8_t _LoopIter,
                       DataGraph::DynamicInstIter _Begin,
                       DataGraph::DynamicInstIter _End);

  void print(llvm::raw_ostream &OStream) const;

  llvm::Loop *Loop;
  const uint8_t LoopIter;
  std::unordered_map<llvm::Instruction *, std::vector<DynamicId>>
      StaticToDynamicMap;
};

class SIMDPass : public ReplayTrace {
public:
  static char ID;
  SIMDPass()
      : ReplayTrace(ID), State(SEARCHING), VectorizeWidth(4),
        VectorizeEfficiencyThreshold(0.8f), Loop(nullptr), LoopIter(0) {
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

  bool doInitialization(llvm::Module &Module) override {
    // Reset memorization.
    this->MemorizeLoopStaticVectorizable.clear();
    this->MemorizeLoopStaticInstCount.clear();
    // Reset other variables.
    this->Loop = nullptr;
    this->LoopIter = 0;
    this->State = SEARCHING;
    // Call the base initialization.
    return ReplayTrace::doInitialization(Module);
  }

protected:
  enum {
    SEARCHING,
    BUFFERING,
  } State;

  uint8_t VectorizeWidth;
  float VectorizeEfficiencyThreshold;

  // Memorization maps.
  std::unordered_map<llvm::Loop *, bool> MemorizeLoopStaticVectorizable;
  std::unordered_map<llvm::Loop *, uint32_t> MemorizeLoopStaticInstCount;

  // Varaibles valid only for BUFFERING state.
  llvm::Loop *Loop;
  uint8_t LoopIter;

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
  float computeLoopEfficiency(llvm::Loop *Loop, uint8_t LoopIter);

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
   * Process the buffered instructions when the buffered iterations reaches
   * VectorizeWidth.
   * May perform actual vectorization or not dependent on the dynamic
   * information. Commit all the buffered instructions EXCEPT THE LAST ONE,
   * which either belongs to next iteration or other regions.
   */
  void processBuffer(std::ofstream &OutTrace);
};

void SIMDPass::transform() {
  assert(this->Trace != nullptr && "Must have a trace to be transformed.");

  std::ofstream OutTrace(this->OutTraceName);
  assert(OutTrace.is_open() && "Failed to open output trace file.");

  // Get the loop info.
  auto &LoopInfo = getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();

  /**
   * A state machine.
   * SEARCHING:
   *    Initial state. Searching for the start of an inner most loop iteration.
   *    If encounter an inner most loop iteration, switch to buffering.
   *    Otherwise, commit this instruction.
   * BUFFERING:
   *    Buffer until some vectorize factor iterations (default 4) for candidate
   *    for transformation. Instructions will not be commited. Once done, check
   *    if the buffered loop is vectorizable. If so, estimate the speed up. If
   *    everything looks fine, transform the datagraph. Otherwise, commit the
   *    buffered instructions.
   */
  this->State = SEARCHING;
  uint64_t Count = 0;

  DEBUG(llvm::errs() << "SIMD Pass start.\n");

  while (true) {

    auto NewDynamicInst = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::BasicBlock *NewBB = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeaderOfVectorizable = false;
    if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      NewBB = NewStaticInst->getParent();
      NewLoop = LoopInfo.getLoopFor(NewBB);

      if (NewLoop && this->isLoopStaticVectorizable(NewLoop)) {

        // Our datagraph skips the phi instruction.
        // Make sure we are hitting the head instruction of inner most loop.
        IsAtHeaderOfVectorizable = isStaticInstLoopHead(NewLoop, NewStaticInst);
      }

    } else {
      // Commit any remaining buffered instructions when hitting the end.
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Trace->DynamicInstructionList.front()->format(OutTrace,
                                                            this->Trace);
        OutTrace << '\n';
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
        this->Trace->DynamicInstructionList.front()->format(OutTrace,
                                                            this->Trace);
        OutTrace << '\n';
        this->Trace->commitOneDynamicInst();
      }

      if (IsAtHeaderOfVectorizable) {
        this->State = BUFFERING;
        this->LoopIter = 0;
        this->Loop = NewLoop;
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
      if (NewLoop != this->Loop) {
        // We are out of the current loop.
        this->LoopIter++;
      } else if (IsAtHeaderOfVectorizable) {
        this->LoopIter++;
      }

      /**
       * Process the buffer if LoopIter reaches vectorize width.
       * This will commit the buffer EXCEPT THE LAST INSTRUCTION.
       */
      if (this->LoopIter == this->VectorizeWidth) {
        // DEBUG(llvm::errs() << "SIMD Pass: Vectorizing.\n");
        this->processBuffer(OutTrace);
        // Clear the loop iter.
        this->LoopIter = 0;
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
      if (NewLoop != this->Loop) {
        // Case 2
        // Commit remaining insts.
        while (this->Trace->DynamicInstructionList.size() > 1) {
          this->Trace->DynamicInstructionList.front()->format(OutTrace,
                                                              this->Trace);
          OutTrace << '\n';
          this->Trace->commitOneDynamicInst();
        }
        if (IsAtHeaderOfVectorizable) {
          // Case 2.1.
          // Update the loop and loop iter.
          this->LoopIter = 0;
          this->Loop = NewLoop;
        } else {
          // Case 2.2.
          // DEBUG(llvm::errs() << "SIMD Pass: Go back to searching state.\n");
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

  OutTrace.close();
}

bool SIMDPass::isLoopStaticVectorizable(llvm::Loop *Loop) {
  auto &Memorized = this->MemorizeLoopStaticVectorizable;

  assert(Loop != nullptr && "Loop should not be nullptr.");

  if (Memorized.find(Loop) != Memorized.end()) {
    return Memorized.at(Loop);
  }

  if (!Loop->empty()) {
    // This is not the inner most loop.
    Memorized.emplace(Loop, false);
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
          Memorized.emplace(Loop, false);
          return false;
        }
        // Check if calling some supported math function.
        if (Callee->getName() != "sin") {
          // TODO: add a set for supported functions.
          Memorized.emplace(Loop, false);
          return false;
        }
      }
    }
  }

  // Done: this loop is statically vectorizable.
  Memorized.emplace(Loop, true);
  return true;
}

float SIMDPass::computeLoopEfficiency(llvm::Loop *Loop, uint8_t LoopIter) {
  if (this->MemorizeLoopStaticInstCount.find(Loop) ==
      this->MemorizeLoopStaticInstCount.end()) {
    uint32_t StaticInstCount = 0;
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      StaticInstCount += (*BBIter)->size();
      // Remember to deduct the phis number.
      auto PHIRange = (*BBIter)->phis();
      for (auto PHIIter = PHIRange.begin(), PHIEnd = PHIRange.end();
           PHIIter != PHIEnd; ++PHIIter) {
        assert(StaticInstCount > 0 &&
               "Should be positive when deduct one phi inst.");
        StaticInstCount--;
      }
    }
    this->MemorizeLoopStaticInstCount.emplace(Loop, StaticInstCount);
  }
  size_t StaticInstCount = this->MemorizeLoopStaticInstCount.at(Loop);

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");
  size_t DynamicInstCount = this->Trace->DynamicInstructionList.size() - 1;
  return static_cast<float>(DynamicInstCount) /
         static_cast<float>(StaticInstCount * LoopIter);
}

bool SIMDPass::isLoopInterIterMemDependent(DynamicInnerMostLoop &DynamicLoop) {

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");

  // Iterate through all the store/load instructions.
  for (auto BBIter = DynamicLoop.Loop->block_begin(),
            BBEnd = DynamicLoop.Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
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

          // Case 2. Check if the dependent dynamic id is in the same iteration.

          // If the dependent dynamic is alive, it must comes from the same
          // loop.
          auto MemDepStaticInst =
              (*(AliveDynamicInstsMapIter->second))->getStaticInstruction();
          assert(DynamicLoop.Loop->contains(MemDepStaticInst) &&
                 "If alive, the memory dependent static instruction should be "
                 "in the same loop.");
          auto &MemDepDynamicIds =
              DynamicLoop.StaticToDynamicMap.at(MemDepStaticInst);
          if (MemDepDynamicIds[CurrentIter] == MemDepId) {
            // The dependent memory instruction is in the same loop. Ignore it.
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

  auto &SE = this->getAnalysis<llvm::ScalarEvolutionWrapperPass>().getSE();

  // Iterate through all the store/load instructions.
  for (auto BBIter = DynamicLoop.Loop->block_begin(),
            BBEnd = DynamicLoop.Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
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

  return false;
}

bool SIMDPass::isLoopDynamicVectorizable(DynamicInnerMostLoop &DynamicLoop) {

  float Efficiency =
      this->computeLoopEfficiency(DynamicLoop.Loop, DynamicLoop.LoopIter);
  if (Efficiency < this->VectorizeEfficiencyThreshold) {
    DEBUG(llvm::errs() << "Failed due to inefficiency\n");
    return false;
  }

  if (this->isLoopInterIterMemDependent(DynamicLoop)) {
    DEBUG(llvm::errs() << "Failed due to inter-iter memory dependent\n");
    return false;
  }

  if (this->isLoopInterIterDataDependent(DynamicLoop)) {
    DEBUG(llvm::errs() << "Failed due to inter-iter data dependent\n");
    return false;
  }

  return true;
}

void SIMDPass::processBuffer(std::ofstream &OutTrace) {

  assert(this->Trace->DynamicInstructionList.size() > 1 &&
         "Should be at least 2 dynamic instruction.");

  // Construct the internal data struct.
  auto Begin = this->Trace->DynamicInstructionList.begin();
  auto End = this->Trace->DynamicInstructionList.end();
  --End;
  DynamicInnerMostLoop DynamicLoop(this->Loop, this->LoopIter, Begin, End);

  if (!this->isLoopDynamicVectorizable(DynamicLoop)) {
    // This loop is not dynamically vectorizable, simply commit it.
    DEBUG(llvm::errs() << "SIMD Pass: Loop is not dynamically vectorizable.\n");
    DEBUG(DynamicLoop.print(llvm::errs()));
    while (this->Trace->DynamicInstructionList.size() > 1) {
      this->Trace->DynamicInstructionList.front()->format(OutTrace,
                                                          this->Trace);
      OutTrace << '\n';
      this->Trace->commitOneDynamicInst();
    }
    return;
  }

  DEBUG(llvm::errs() << "SIMD Pass: Transforming Loop.===========\n");
  DEBUG(DynamicLoop.print(llvm::errs()));

  // For now simply commit everything.
  while (this->Trace->DynamicInstructionList.size() > 1) {
    this->Trace->DynamicInstructionList.front()->format(OutTrace, this->Trace);
    OutTrace << '\n';
    this->Trace->commitOneDynamicInst();
  }
}

DynamicInnerMostLoop::DynamicInnerMostLoop(llvm::Loop *_Loop, uint8_t _LoopIter,
                                           DataGraph::DynamicInstIter _Begin,
                                           DataGraph::DynamicInstIter _End)
    : Loop(_Loop), LoopIter(_LoopIter) {

  assert(this->Loop != nullptr && "DynamicInnerMostLoop: Null loop.");
  assert(this->Loop->empty() && "The loop should be inner most.");

  /**
   * Create the map at the beginning. This will ensure that we have the vector
   * for all the static instructions inside the loop.
   */
  for (auto BBIter = this->Loop->block_begin(), BBEnd = this->Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
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
    assert(this->Loop->contains(StaticInst) &&
           "The static inst is out of the loop.");

    if (isStaticInstLoopHead(Loop, StaticInst)) {
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
  OStream << "Name: " << this->Loop->getHeader()->getParent()->getName()
          << "::" << this->Loop->getName()
          << " Iter: " << static_cast<int>(this->LoopIter) << '\n';
}

} // namespace

#undef DEBUG_TYPE

char SIMDPass::ID = 0;
static llvm::RegisterPass<SIMDPass> X("simd-pass", "SIMD transform pass", false,
                                      false);
