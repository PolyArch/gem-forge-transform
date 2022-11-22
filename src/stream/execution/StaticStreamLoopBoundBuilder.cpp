#include "StaticStreamLoopBoundBuilder.h"

#define DEBUG_TYPE "StaticStreamLoopBoundBuilder"

StaticStreamLoopBoundBuilder::StaticStreamLoopBoundBuilder(
    StreamExecutionTransformer *_Transformer)
    : Transformer(_Transformer) {}

void StaticStreamLoopBoundBuilder::buildStreamLoopBoundForLoop(
    StaticStreamRegionAnalyzer *Analyzer, const llvm::Loop *Loop,
    InputValueVec &ClonedInputValues) {
  if (!StreamPassEnableLoopBoundPredication) {
    return;
  }
  LLVM_DEBUG(llvm::dbgs() << "[LoopBound] Try to build StreamLoopBound for "
                          << LoopUtils::getLoopId(Loop) << "\n");

  auto SE = this->Transformer->CachedLI->getScalarEvolution(
      Loop->getHeader()->getParent());
  auto TripCountSCEV = LoopUtils::getTripCountSCEV(SE, Loop);
  if (!llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV) &&
      SE->isLoopInvariant(TripCountSCEV, Loop)) {
    LLVM_DEBUG({
      llvm::dbgs()
          << "[LoopBound] Skip StreamLoopBound for FixedTripCountSCEV ";
      TripCountSCEV->print(llvm::dbgs());
      llvm::dbgs() << "\n";
    });
    return;
  }

  auto LatchBB = Loop->getLoopLatch();
  if (!LatchBB) {
    LLVM_DEBUG(llvm::dbgs() << "[LoopBound] No Latch.\n");
    return;
  }
  // This is the single latch.
  auto IsStreamInst = [Analyzer](const llvm::Instruction *Inst) -> bool {
    return Analyzer->isStreamInst(Inst);
  };
  auto LatchBBBranchDG = Analyzer->getBBBranchDG()->getBBBranchDataGraph(
      Loop, LatchBB, IsStreamInst);
  if (!this->isStreamLoopBound(Analyzer, Loop, LatchBBBranchDG)) {
    LLVM_DEBUG(llvm::dbgs() << "[LoopBound] Not StreamLoopBound.\n");
    return;
  }

  // Break out the loop when DB() == Ret.
  bool LoopBoundRet = true;
  auto TrueBB = LatchBBBranchDG->getTrueBB();
  auto FalseBB = LatchBBBranchDG->getFalseBB();
  if (TrueBB && Loop->getHeader() == TrueBB) {
    // True is back edge.
    LoopBoundRet = false;
  }

  // Build the ExecFuncInfo.
  auto LoopBoundFuncInfo = std::make_unique<::LLVM::TDG::ExecFuncInfo>();
  LoopBoundFuncInfo->set_name(LatchBBBranchDG->getFuncName());
  LoopBoundFuncInfo->set_type(::LLVM::TDG::DataType::INTEGER);
  for (auto Input : LatchBBBranchDG->getInputs()) {
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Input);
    auto FuncArg = LoopBoundFuncInfo->add_args();
    if (!Inst || !Loop->contains(Inst)) {
      // This is a non-stream input.
      FuncArg->set_is_stream(false);
      FuncArg->set_type(StaticStream::translateToProtobufDataType(
          this->Transformer->ClonedDataLayout.get(), Input->getType()));
      ClonedInputValues.push_back(this->Transformer->getClonedValue(Input));
      continue;
    }
    auto S = Analyzer->getChosenStreamWithPlaceholderInst(Inst);
    assert(S && "Failed to get LoopBoundInputS.");
    FuncArg->set_is_stream(true);
    FuncArg->set_stream_id(S->StreamId);
    FuncArg->set_type(StaticStream::translateToProtobufDataType(
        this->Transformer->ClonedDataLayout.get(), Inst->getType()));
  }

  LLVM_DEBUG(llvm::dbgs() << "[LoopBound] Add LoopBound to Loop "
                          << LoopUtils::getLoopId(Loop) << "\n");

  auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  ConfigureInfo.addLoopBoundDG(std::move(LoopBoundFuncInfo), LoopBoundRet);
}

bool StaticStreamLoopBoundBuilder::isStreamLoopBound(
    StaticStreamRegionAnalyzer *Analyzer, const llvm::Loop *Loop,
    BBBranchDataGraph *LatchBBBranchDG) {
  /**
   * We add the LoopBoundDG to ConfigureInfo iff.
   * 1. This is a valid LoopBoundPredicate.
   * 2. All the input streams are chosen streams configured at this loop.
   */
  if (!LatchBBBranchDG->isValidLoopBoundPredicate()) {
    return false;
  }
  for (auto Input : LatchBBBranchDG->getInLoopInputs()) {
    auto S = Analyzer->getChosenStreamWithPlaceholderInst(Input);
    if (!S) {
      return false;
    }
    if (S->ConfigureLoop != Loop) {
      return false;
    }
  }
  return true;
}