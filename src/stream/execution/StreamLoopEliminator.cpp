#include "StreamLoopEliminator.h"

#include "llvm/IR/Verifier.h"

#define DEBUG_TYPE "StreamLoopEliminator"

StreamLoopEliminator::StreamLoopEliminator(
    StreamExecutionTransformer *_Transformer)
    : Transformer(_Transformer) {}

void StreamLoopEliminator::eliminateLoops(
    StaticStreamRegionAnalyzer *Analyzer) {
  if (!StreamPassEnableLoopElimination) {
    return;
  }
  this->eliminateLoop(Analyzer, Analyzer->getTopLoop());
}

void StreamLoopEliminator::eliminateLoop(StaticStreamRegionAnalyzer *Analyzer,
                                         const llvm::Loop *Loop) {
  for (auto SubLoop : Loop->getSubLoops()) {
    this->eliminateLoop(Analyzer, SubLoop);
  }
  LLVM_DEBUG(llvm::dbgs() << "[LoopEliminate] Try eliminating Loop "
                          << LoopUtils::getLoopId(Loop) << ".\n");

  auto LoopHeader = Loop->getHeader();
  auto ClonedLoopHeader = this->Transformer->getClonedValue(LoopHeader);
  auto ClonedFunc = ClonedLoopHeader->getParent();

  auto ClonedLI = this->Transformer->CachedLI->getLoopInfo(ClonedFunc);
  auto ClonedLoop = ClonedLI->getLoopFor(ClonedLoopHeader);
  if (!ClonedLoop) {
    // 0. Still has the ClonedLoop.
    LLVM_DEBUG(llvm::dbgs() << "[LoopEliminate] Missing ClonedLoop.\n");
    return;
  }

  if (!this->canLoopBeEliminated(Analyzer, Loop, ClonedLoop)) {
    return;
  }
  /**
   * To eliminate a loop, we perform these steps:
   * 1. Get the preheader and the single dedicate exit block.
   * 2. Clear all the buffered analysis on this cloned function.
   * 3. Let the preheader directly branch into the single exit.
   * 4. Remove preheader from header's predecessor list.
   * 5. Remember that this loop is eliminated.
   */
  auto ClonedPreheaderBB = ClonedLoop->getLoopPreheader();
  auto ClonedSingleExitBB = ClonedLoop->getExitBlock();

  // Remember the eliminated BB.
  for (auto ClonedBB : ClonedLoop->blocks()) {
    this->EliminatedBBs.insert(ClonedBB);
  }

  this->Transformer->CachedLI->clearFuncAnalysis(ClonedFunc);

  auto ClonedPreheaderTerminator = ClonedPreheaderBB->getTerminator();
  auto ClonedPreheaderBr =
      llvm::dyn_cast<llvm::BranchInst>(ClonedPreheaderTerminator);
  if (!ClonedPreheaderBr) {
    llvm::errs()
        << "[LoopEliminate] Preheader Terminator should be BranchInst. Got:\n";
    llvm::errs() << "[LoopEliminate] ";
    ClonedPreheaderTerminator->print(llvm::errs(), true /* IsForDebug */);
    llvm::errs() << "\n";
  }
  assert(ClonedPreheaderBr->getNumSuccessors() == 1 &&
         "Preheader should have only one successor.");
  LLVM_DEBUG({
    llvm::dbgs() << "[LoopEliminate] Remove Preheader "
                 << ClonedPreheaderBB->getName() << " as Predecessor from "
                 << ClonedLoopHeader->getName() << ".\n";
  });
  ClonedLoopHeader->removePredecessor(ClonedPreheaderBB, true
                                      /* KeepOneInputPHIs */);
  LLVM_DEBUG({
    llvm::dbgs() << "[LoopEliminate] Branch Preheader "
                 << ClonedPreheaderBB->getName() << " -> "
                 << ClonedSingleExitBB->getName() << ".\n";
  });
  ClonedPreheaderBr->setSuccessor(0, ClonedSingleExitBB);

  auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  ConfigureInfo.setLoopEliminated(true);
  for (auto S : ConfigureInfo.getSortedStreams()) {
    // Eliminated Loop has no core users.
    S->StaticStreamInfo.set_no_core_user(true);
  }

  LLVM_DEBUG({
    if (llvm::verifyFunction(*ClonedFunc, &llvm::dbgs())) {
      llvm::errs() << "Func broken after eliminating StreamLoop.\n";
      assert(false && "Func broken.");
    }
  });
}

bool StreamLoopEliminator::canLoopBeEliminated(
    StaticStreamRegionAnalyzer *Analyzer, const llvm::Loop *Loop,
    const llvm::Loop *ClonedLoop) {
  /**
   * A loop can be eliminated iff:
   * 0. Still has the ClonedLoop.
   * 1. For all ClonedInsts, must be one of these:
   *   a. Pending removed.
   *   b. No outside user and
   *      Not Call/Invoke/Store, except StreamLoad/StreamStep.
   * 2. Has StreamLoopBound or FixedTripCountSCEV.
   * 3. Has a preheader and single dedicated exit block.
   */

  for (auto *BB : ClonedLoop->blocks()) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto *Inst = &*InstIter;
      if (this->Transformer->PendingRemovedInsts.count(Inst)) {
        // 1a.
        continue;
      }
      for (auto User : Inst->users()) {
        if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
          // Check if the UserInst is from eliminated subloop.
          if (!ClonedLoop->contains(UserInst) &&
              !this->isBBElminated(UserInst->getParent())) {
            // 1b.
            LLVM_DEBUG({
              llvm::dbgs() << "[LoopEliminate] Outside User for Inst:\n";
              llvm::dbgs() << "[LoopEliminate] ";
              Inst->print(llvm::dbgs(), true /* IsForDebug */);
              llvm::dbgs() << "\n";
              llvm::dbgs() << "[LoopEliminate] Outside User:\n";
              llvm::dbgs() << "[LoopEliminate] ";
              UserInst->print(llvm::dbgs(), true /* IsForDebug */);
              llvm::dbgs() << "\n";
            });
            return false;
          }
        }
      }
      if (Utils::isCallOrInvokeInst(Inst)) {
        // 1c.
        auto Callee = Utils::getCalledFunction(Inst);
        if (Callee &&
            (Callee->getIntrinsicID() == llvm::Intrinsic::ssp_stream_load ||
             Callee->getIntrinsicID() == llvm::Intrinsic::ssp_stream_step ||
             Callee->getIntrinsicID() == llvm::Intrinsic::ssp_stream_atomic ||
             Callee->getIntrinsicID() == llvm::Intrinsic::ssp_stream_store)) {
          // This is Okay.
        } else {
          LLVM_DEBUG({
            llvm::dbgs() << "[LoopEliminate] Invalid Call/Invoke Inst ";
            Inst->print(llvm::dbgs(), true /* IsForDebug */);
            llvm::dbgs() << "\n";
          });
          return false;
        }
      } else if (llvm::isa<llvm::StoreInst>(Inst)) {
        LLVM_DEBUG({
          llvm::dbgs() << "[LoopEliminate] Invalid Store Inst ";
          Inst->print(llvm::dbgs(), true /* IsForDebug */);
          llvm::dbgs() << "\n";
        });
        return false;
      }
    }
  }

  auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  if (!ConfigureInfo.hasLoopBoundDG()) {
    // 2.
    LLVM_DEBUG({
      llvm::dbgs()
          << "[LoopEliminate] No StreamLoopBound. Check TripCountSCEV.\n";
    });

    auto SE = this->Transformer->CachedLI->getScalarEvolution(
        Loop->getHeader()->getParent());
    auto TripCountSCEV = LoopUtils::getTripCountSCEV(SE, Loop);
    if (llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV) ||
        (!SE->isLoopInvariant(TripCountSCEV, Loop))) {

      LLVM_DEBUG(
          { llvm::dbgs() << "[LoopEliminate] No FixedTripCountSCEV.\n"; });
      return false;
    }
  }

  if (!ClonedLoop->getLoopPreheader()) {
    // 3a.
    LLVM_DEBUG({ llvm::dbgs() << "[LoopEliminate] No PreHeader.\n"; });
    return false;
  }

  if (auto ExitBB = ClonedLoop->getExitBlock()) {
    if (!ExitBB->getSinglePredecessor()) {
      // 3b.
      LLVM_DEBUG({ llvm::dbgs() << "[LoopEliminate] No Dedicate ExitBB.\n"; });
      return false;
    }
  } else {
    // 3b.
    LLVM_DEBUG({ llvm::dbgs() << "[LoopEliminate] Multiple ExitBB.\n"; });
    return false;
  }

  // if (ClonedLoop->getNumBlocks() != 1) {
  //   // 4.
  //   LLVM_DEBUG({ llvm::dbgs() << "[LoopEliminate] Multiple BBs.\n"; });
  //   return false;
  // }

  LLVM_DEBUG(llvm::dbgs() << "[LoopEliminate] Can eliminate Loop "
                          << LoopUtils::getLoopId(Loop) << ".\n");
  return true;
}
