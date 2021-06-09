
#include "StaticNestStreamBuilder.h"
#include "NestStreamConfigureDataGraph.h"

#define DEBUG_TYPE "StaticNestStreamBuilder"

StaticNestStreamBuilder::StaticNestStreamBuilder(
    StreamExecutionTransformer *_Transformer)
    : Transformer(_Transformer) {}

void StaticNestStreamBuilder::buildNestStreams(
    StaticStreamRegionAnalyzer *Analyzer) {
  if (!StreamPassEnableNestStream) {
    return;
  }
  /**
   * We require preorder to iterate through all streams.
   * This means we first nest outer streams, then inner ones.
   */
  auto TopLoop = Analyzer->getTopLoop();
  for (auto SubLoop : TopLoop->getLoopsInPreorder()) {
    this->buildNestStreamsForLoop(Analyzer, SubLoop);
  }
}

void StaticNestStreamBuilder::buildNestStreamsForLoop(
    StaticStreamRegionAnalyzer *Analyzer, const llvm::Loop *Loop) {
  // Searching from TopLoops.
  auto TopLoop = Analyzer->getTopLoop();
  std::vector<const llvm::Loop *> OuterLoops;
  for (auto OuterLoop = Loop->getParentLoop();
       OuterLoop && TopLoop->contains(OuterLoop);
       OuterLoop = OuterLoop->getParentLoop()) {
    OuterLoops.emplace_back(OuterLoop);
  }
  for (auto Iter = OuterLoops.rbegin(), End = OuterLoops.rend(); Iter != End;
       ++Iter) {
    auto OuterLoop = *Iter;
    if (this->canStreamsBeNested(Analyzer, OuterLoop, Loop)) {
      // We have decided
      break;
    }
  }
}

bool StaticNestStreamBuilder::canStreamsBeNested(
    StaticStreamRegionAnalyzer *Analyzer, const llvm::Loop *OuterLoop,
    const llvm::Loop *InnerLoop) {
  /**
   * This function checks if streams configured at InnerLoop can be nested into
   * the OuterLoop We first define two set of streams:
   * 1. InnerStreams: streams that are configured at the InnerLoop.
   * 2. OuterStreams: streams that are configured at the OuterLoop.
   *
   * We check the following conditions:
   * 1. All parameters of InnerStreams are from OuterStreams.
   * 2. All InnerStreams and OuterStreams have known trip count.
   * 3. The conditional branch entering InnerLoop shoud be evaluated using
   * OuterStreams.
   */

  LLVM_DEBUG(llvm::dbgs() << "[Nest] Check nest condition for InnerLoop "
                          << LoopUtils::getLoopId(InnerLoop) << ", OuterLoop "
                          << LoopUtils::getLoopId(OuterLoop) << '\n');

  const auto &OuterConfigInfo = Analyzer->getConfigureLoopInfo(OuterLoop);
  const auto &InnerConfigInfo = Analyzer->getConfigureLoopInfo(InnerLoop);

  if (InnerConfigInfo.getSortedStreams().empty()) {
    return false;
  }

  LLVM_DEBUG({
    for (auto S : InnerConfigInfo.getSortedStreams()) {
      llvm::dbgs() << "[Nest] Found InnerStream: " << S->formatName() << '\n';
    }
    for (auto S : OuterConfigInfo.getSortedStreams()) {
      llvm::dbgs() << "[Nest] Found OuterStream: " << S->formatName() << '\n';
    }
  });

  auto ClonedInnerConfigureBB =
      this->Transformer->getClonedConfigureBBForLoop(InnerLoop);
  auto ClonedOuterLoop =
      this->Transformer->getOrCreateLoopInClonedModule(OuterLoop);
  auto NestConfigureDataGraph = std::make_unique<NestStreamConfigureDataGraph>(
      ClonedOuterLoop, ClonedInnerConfigureBB);

  LLVM_DEBUG(llvm::dbgs() << "[Nest] NestConfigureDataGraph Valid? "
                          << NestConfigureDataGraph->isValid() << '\n');
  if (!NestConfigureDataGraph->isValid()) {
    return false;
  }

  /**
   * Try to construct the ExecFuncInfo. This will check that all StreamLoad
   * inputs are from streams configured in OuterLoop.
   */
  auto NestConfigureFuncInfo = std::make_unique<::LLVM::TDG::ExecFuncInfo>();
  NestConfigureFuncInfo->set_name(NestConfigureDataGraph->getFuncName());
  NestConfigureFuncInfo->set_type(::LLVM::TDG::DataType::VOID);
  std::vector<StaticStream *> InputStreams;
  std::vector<const llvm::Value *> InputValues;
  for (auto Input : NestConfigureDataGraph->getInputs()) {
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Input);
    auto NestConfigureFuncArg = NestConfigureFuncInfo->add_args();
    if (!Inst || !ClonedOuterLoop->contains(Inst)) {
      // This is a non-stream input.
      NestConfigureFuncArg->set_is_stream(false);
      NestConfigureFuncArg->set_type(StaticStream::translateToProtobufDataType(
          this->Transformer->ClonedDataLayout.get(), Input->getType()));
      InputValues.push_back(Input);
      continue;
    }
    auto IntrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(Inst);
    assert(IntrinsicInst && "This should be a call to StreamLoad.");
    assert(IntrinsicInst->getIntrinsicID() ==
               llvm::Intrinsic::ssp_stream_load &&
           "Callee should be StreamLoad.");
    auto StreamRegionIdValue =
        llvm::dyn_cast<llvm::ConstantInt>(IntrinsicInst->getArgOperand(0));
    assert(StreamRegionIdValue && "Failed to get RegionStreamId.");
    auto StreamRegionId = StreamRegionIdValue->getZExtValue();
    bool FoundStream = false;
    for (auto S : OuterConfigInfo.getSortedStreams()) {
      if (S->getRegionStreamId() == StreamRegionId) {
        InputStreams.emplace_back(S);
        NestConfigureFuncArg->set_is_stream(true);
        NestConfigureFuncArg->set_stream_id(S->StreamId);
        NestConfigureFuncArg->set_type(
            StaticStream::translateToProtobufDataType(
                this->Transformer->ClonedDataLayout.get(), Input->getType()));
        FoundStream = true;
        break;
      }
    }
    if (!FoundStream) {
      LLVM_DEBUG(llvm::dbgs()
                 << "[Nest] StreamLoad input not from outer loop.");
      return false;
    }
  }
  if (InputStreams.empty()) {
    LLVM_DEBUG(llvm::dbgs() << "[Nest] At least one StreamLoad input.\n");
    return false;
  }

  /**
   * We still need to check the conditional branch.
   */
  auto OuterHeaderBB = OuterLoop->getHeader();
  auto InnerHeaderBB = InnerLoop->getHeader();
  auto BBPredDG = Analyzer->getBBPredDG()->getBBPredicateDataGraph(
      OuterLoop, OuterHeaderBB);
  bool IsPredicated = false;
  bool PredicateRet = false;
  auto PredFuncInfo = std::make_unique<::LLVM::TDG::ExecFuncInfo>();
  std::vector<const llvm::Value *> PredInputValues;
  if (BBPredDG->isValid() && (BBPredDG->getTrueBB() == InnerHeaderBB ||
                              BBPredDG->getFalseBB() == InnerHeaderBB)) {
    /**
     * This is predicated, check that all inputs are from streams.
     * And construct the input values and FuncInfo.
     */
    PredFuncInfo->set_name(BBPredDG->getFuncName());
    PredFuncInfo->set_type(::LLVM::TDG::DataType::INTEGER);
    for (auto Input : BBPredDG->getInputs()) {
      auto Inst = llvm::dyn_cast<llvm::Instruction>(Input);
      auto PredFuncArg = PredFuncInfo->add_args();
      if (!Inst || !OuterLoop->contains(Inst)) {
        // This is a non-stream input.
        PredFuncArg->set_is_stream(false);
        PredFuncArg->set_type(StaticStream::translateToProtobufDataType(
            this->Transformer->ClonedDataLayout.get(), Input->getType()));
        PredInputValues.push_back(this->Transformer->getClonedValue(Input));
        continue;
      }
      auto S = Analyzer->getChosenStreamByInst(Inst);
      if (!S || S->ConfigureLoop != OuterLoop || !S->isChosen()) {
        LLVM_DEBUG(llvm::dbgs()
                   << "[Nest] Predication has non-stream load input "
                   << Utils::formatLLVMInst(Inst) << ".\n");
        return false;
      }
      PredFuncArg->set_is_stream(true);
      PredFuncArg->set_stream_id(S->StreamId);
      PredFuncArg->set_type(StaticStream::translateToProtobufDataType(
          this->Transformer->ClonedDataLayout.get(), Inst->getType()));
    }
    IsPredicated = true;
    if (BBPredDG->getTrueBB() == InnerHeaderBB) {
      PredicateRet = true;
    } else {
      PredicateRet = false;
    }
  }
  if (!IsPredicated && !Analyzer->getPostDominatorTree()->dominates(
                           InnerHeaderBB, OuterHeaderBB)) {
    LLVM_DEBUG(llvm::dbgs() << "[Nest] Is not predicated or post-dominated.\n");
    return false;
  }
  if (!IsPredicated) {
    PredFuncInfo = nullptr;
    PredInputValues.clear();
  }

  /**
   * Everything checks out, we can try to transform the program.
   * 1. Insert this configure function in the cloned module.
   * 2. Mark all the StreamConfig/Input/Ready/End pending removed.
   * 3. Insert any additional input for the configure function.
   * 4. Remember the ExecFunc in the StreamRegionConfigureInfo.
   * 5. Do the same thing for the predicate function.
   */

  NestConfigureDataGraph->generateFunction(
      NestConfigureDataGraph->getFuncName(), this->Transformer->ClonedModule);

  for (auto InstIter = ClonedInnerConfigureBB->begin(),
            InstEnd = ClonedInnerConfigureBB->end();
       InstIter != InstEnd; ++InstIter) {
    auto Inst = &*InstIter;
    if (auto IntrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(Inst)) {
      switch (IntrinsicInst->getIntrinsicID()) {
      default:
        break;
      case llvm::Intrinsic::ssp_stream_config:
      case llvm::Intrinsic::ssp_stream_input:
      case llvm::Intrinsic::ssp_stream_ready: {
        this->Transformer->PendingRemovedInsts.insert(Inst);
        break;
      }
      }
    }
  }
  // TODO: For now I keep the StreamEnd for simplicity
  // for (auto StreamEndInst :
  //      this->Transformer->getClonedEndInstsForLoop(InnerLoop)) {
  //   this->Transformer->PendingRemovedInsts.insert(StreamEndInst);
  // }

  auto ClonedOuterConfigureBB =
      this->Transformer->getClonedConfigureBBForLoop(OuterLoop);
  llvm::Instruction *ClonedOuterStreamReadyInst = nullptr;
  for (auto InstIter = ClonedOuterConfigureBB->begin(),
            InstEnd = ClonedOuterConfigureBB->end();
       InstIter != InstEnd; ++InstIter) {
    auto Inst = &*InstIter;
    if (auto IntrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(Inst)) {
      auto IntrinsicID = IntrinsicInst->getIntrinsicID();
      if (IntrinsicID == llvm::Intrinsic::ssp_stream_ready) {
        ClonedOuterStreamReadyInst = IntrinsicInst;
        break;
      }
    }
  }
  assert(ClonedOuterStreamReadyInst &&
         "Failed to find StreamReady for OuterLoop.");
  llvm::IRBuilder<> Builder(ClonedOuterStreamReadyInst);
  for (auto InputValue : InputValues) {
    this->Transformer->addStreamInput(
        Builder,
        ::LLVM::TDG::ReservedStreamRegionId::NestConfigureFuncInputRegionId,
        const_cast<llvm::Value *>(InputValue));
  }
  // Push input for the predication.
  for (auto InputValue : PredInputValues) {
    this->Transformer->addStreamInput(
        Builder,
        ::LLVM::TDG::ReservedStreamRegionId::NestConfigureFuncInputRegionId,
        const_cast<llvm::Value *>(InputValue));
  }

  Analyzer->nestRegionInto(InnerLoop, OuterLoop,
                           std::move(NestConfigureFuncInfo),
                           std::move(PredFuncInfo), PredicateRet);

  return true;
}