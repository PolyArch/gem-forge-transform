/**
 * This pass will apply SIMD transformation to the datagraph.
 */

#include "Replay.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
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

#define DEBUG_TYPE "SIMDPass"
namespace {

class SIMDPass : public ReplayTrace {
public:
  static char ID;
  SIMDPass()
      : ReplayTrace(ID), State(SEARCHING), VectorizeWidth(4), Loop(nullptr),
        LoopIter(0) {
    if (::VectorizeWidth.getNumOccurrences() > 0) {
      this->VectorizeWidth = ::VectorizeWidth.getValue();
    }
  }

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    ReplayTrace::getAnalysisUsage(Info);
    // We require the loop information. We can not preserve it
    // as we may modify some function body.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
  }

protected:
  enum {
    SEARCHING,
    BUFFERING,
  } State;

  uint8_t VectorizeWidth;

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

    DEBUG(llvm::errs() << "Loaded one instruction.\n");

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::BasicBlock *NewBB = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeaderOfVectorizable = false;
    if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      DEBUG(llvm::errs() << "Loaded " << NewStaticInst->getName() << '\n');

      NewBB = NewStaticInst->getParent();
      NewLoop = LoopInfo.getLoopFor(NewBB);

      if (NewLoop && this->isLoopStaticVectorizable(NewLoop)) {

        // Our datagraph skips the phi instruction.
        // Make sure we are hitting the head instruction of inner most loop.
        IsAtHeaderOfVectorizable = ((NewLoop->getHeader() == NewBB) &&
                                    (NewBB->getFirstNonPHI() == NewStaticInst));
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

    DEBUG(llvm::errs() << "Start working.\n");

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
        DEBUG(llvm::errs() << "SIMD Pass: Search state commit immediately."
                           << "\n");
        this->Trace->DynamicInstructionList.front()->format(OutTrace,
                                                            this->Trace);
        DEBUG(llvm::errs() << "SIMD Pass: Search state commit done.\n");
        OutTrace << '\n';
        this->Trace->commitOneDynamicInst();
      }

      if (IsAtHeaderOfVectorizable) {
        DEBUG(llvm::errs() << "SIMD Pass: Transit to buffering for BB "
                           << NewBB->getName() << '\n');
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
        DEBUG(llvm::errs() << "SIMD Pass: Vectorizing.\n");
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
          DEBUG(llvm::errs() << "SIMD Pass: Go back to searching state.\n");
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

    DEBUG(llvm::errs() << "Done working.\n");
  }

  DEBUG(llvm::errs() << "SIMD Pass end.\n");

  OutTrace.close();
} // namespace

void SIMDPass::processBuffer(std::ofstream &OutTrace) {
  // For now simply commit everything.
  while (this->Trace->DynamicInstructionList.size() > 1) {
    this->Trace->DynamicInstructionList.front()->format(OutTrace, this->Trace);
    OutTrace << '\n';
    this->Trace->commitOneDynamicInst();
  }
}

bool SIMDPass::isLoopStaticVectorizable(llvm::Loop *Loop) {
  static std::unordered_map<llvm::Loop *, bool> Memorized;

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

} // namespace

#undef DEBUG_TYPE

char SIMDPass::ID = 0;
static llvm::RegisterPass<SIMDPass> X("simd-pass", "SIMD transform pass", false,
                                      false);
