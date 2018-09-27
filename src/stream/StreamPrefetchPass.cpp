#include "stream/StreamPass.h"

#define DEBUG_TYPE "StreamPrefetchPass"

namespace {

/**
 * This is an experimental pass to evaluate the performance of
 * using stream pattern as prefetcher only.
 * This is done by inserting configure and step instructions, only,
 * whithout user instruction transformed and removable instruction deleted.
 */
class StreamPrefetchPass : public StreamPass {
public:
  static char ID;
  StreamPrefetchPass() : StreamPass(ID) {}

protected:
  void transformStream() override;
};

void StreamPrefetchPass::transformStream() {
  DEBUG(llvm::errs() << "StreamPrefetch: Start transform.\n");

  LoopStackT LoopStack;
  ActiveStreamInstMapT ActiveStreamInstMap;

  while (true) {
    auto NewInstIter = this->Trace->loadOneDynamicInst();

    DynamicInstruction *NewDynamicInst = nullptr;
    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeadOfCandidate = false;

    while (this->Trace->DynamicInstructionList.size() > 10) {
      auto DynamicInst = this->Trace->DynamicInstructionList.front();
      // Debug a certain range of transformed instructions.
      if (DynamicInst->getId() > 19923000 && DynamicInst->getId() < 19923700) {
        DEBUG(this->DEBUG_TRANSFORMED_STREAM(DynamicInst));
      }
      this->Serializer->serialize(DynamicInst, this->Trace);
      this->Trace->commitOneDynamicInst();
    }

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewDynamicInst = *NewInstIter;
      NewStaticInst = NewDynamicInst->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        IsAtHeadOfCandidate =
            LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst) &&
            this->isLoopContinuous(NewLoop);
      }
    } else {
      // This is the end.
      while (!LoopStack.empty()) {
        this->popLoopStackAndUnconfigureStreams(LoopStack, ActiveStreamInstMap);
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      break;
    }

    // First manager the loop stack.
    while (!LoopStack.empty()) {
      if (LoopStack.back()->contains(NewStaticInst)) {
        break;
      }
      // No special handling when popping loop stack.
      this->popLoopStackAndUnconfigureStreams(LoopStack, ActiveStreamInstMap);
    }

    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back() != NewLoop) {
        // A new loop. We should configure all the streams here.
        this->pushLoopStackAndConfigureStreams(LoopStack, NewLoop, NewInstIter,
                                               ActiveStreamInstMap);
      } else {
        // This means that we are at a new iteration.
      }
    }

    // Update the loaded value for functional stream engine.
    if (llvm::isa<llvm::LoadInst>(NewStaticInst)) {
      auto ChosenInstStreamIter = this->ChosenInstStreamMap.find(NewStaticInst);
      if (ChosenInstStreamIter != this->ChosenInstStreamMap.end()) {
        auto S = ChosenInstStreamIter->second;
        this->FuncSE->updateLoadedValue(S, this->Trace,
                                        *(NewDynamicInst->DynamicResult));
      }
    }

    auto PlanIter = this->InstPlanMap.find(NewStaticInst);
    if (PlanIter == this->InstPlanMap.end()) {
      continue;
    }

    const auto &TransformPlan = PlanIter->second;
    if (TransformPlan.Plan == StreamTransformPlan::PlanT::DELETE) {
      /**
       * No more handling for the deleted instruction.
       */
      continue;
    } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STEP) {

      // Inform the functional stream engine to step.
      this->FuncSE->step(TransformPlan.getParamStream(), this->Trace);

      /**
       * We insert the step instruction after the actual step instruction to
       * inform the stream engine move forward.
       */
      auto NewDynamicId = NewDynamicInst->getId();

      auto StepInst = new StreamStepInst(TransformPlan.getParamStream());
      auto StepDynamicId = StepInst->getId();
      // Insert after the actual step inst.
      auto InsertBefore = NewInstIter;
      ++InsertBefore;
      auto StepInstIter =
          this->Trace->insertDynamicInst(InsertBefore, StepInst);

      /**
       * Handle the dependence for the step instruction.
       * It should be dependent on the previous step/config inst of the same
       * stream, as well as the actual step instruction to make step at the
       * correct time.
       */
      auto &RegDeps = this->Trace->RegDeps.at(StepDynamicId);
      // Dependent on the previous step instruction.
      RegDeps.emplace_back(nullptr, NewDynamicId);

      // Dependent on the previous step/config instruction.
      // And also register myself as the latest step instruction for future step
      // instruction.
      auto StreamInst = TransformPlan.getParamStream()->getInst();
      auto StreamInstIter = ActiveStreamInstMap.find(StreamInst);
      if (StreamInstIter == ActiveStreamInstMap.end()) {
        DEBUG(llvm::errs() << "Missing dependence for step instruction.\n");
        ActiveStreamInstMap.emplace(StreamInst, StepDynamicId);
      } else {
        RegDeps.emplace_back(nullptr, StreamInstIter->second);
        StreamInstIter->second = StepDynamicId;
      }

      this->StepInstCount++;
      continue;

    } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STORE) {

      assert(llvm::isa<llvm::StoreInst>(NewStaticInst) &&
             "STORE plan for non store instruction.");
      /**
       * No need to handle stream store in prefetch pass.
       */
    }

    /**
     * Do not handle the use information.
     */
  }

  DEBUG(llvm::errs() << "StreamPrefetch: Transform done.\n");
}
} // namespace

#undef DEBUG_TYPE

char StreamPrefetchPass::ID = 0;
static llvm::RegisterPass<StreamPrefetchPass>
    X("stream-prefetch-pass", "Stream prefetch transform pass", false, false);