#include "stream/StreamPass.h"

#define DEBUG_TYPE "StreamPrefetchPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

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
  LLVM_DEBUG(llvm::errs() << "StreamPrefetch: Start transform.\n");

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
            this->isLoopCandidate(NewLoop);
      }
    } else {
      // This is the end.
      while (!LoopStack.empty()) {
        this->popLoopStackAndUnconfigureStreams(LoopStack, NewInstIter,
                                                ActiveStreamInstMap);
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      /**
       * Notify all region analyzer to wrap up.
       */
      for (auto &LoopStreamAnalyzer : this->LoopStreamAnalyzerMap) {
        LoopStreamAnalyzer.second->getFuncSE()->finalizeCoalesceInfo();
      }
      break;
    }

    // First manager the loop stack.
    while (!LoopStack.empty()) {
      if (LoopStack.back()->contains(NewStaticInst)) {
        break;
      }
      // No special handling when popping loop stack.
      this->popLoopStackAndUnconfigureStreams(LoopStack, NewInstIter,
                                              ActiveStreamInstMap);
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
    if (!LoopStack.empty()) {
      if (llvm::isa<llvm::LoadInst>(NewStaticInst)) {
        auto S =
            this->CurrentStreamAnalyzer->getChosenStreamByInst(NewStaticInst);
        if (S != nullptr) {
          this->CurrentStreamAnalyzer->getFuncSE()->updateWithValue(
              S, this->Trace, *(NewDynamicInst->DynamicResult));
        }
      }

      // Update the phi node value for functional stream engine.
      for (unsigned OperandIdx = 0,
                    NumOperands = NewStaticInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto OperandValue = NewStaticInst->getOperand(OperandIdx);
        if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {
          auto S = this->CurrentStreamAnalyzer->getChosenStreamByInst(PHINode);
          if (S != nullptr) {
            this->CurrentStreamAnalyzer->getFuncSE()->updateWithValue(
                S, this->Trace, *(NewDynamicInst->DynamicOperands[OperandIdx]));
          }
        }
      }

      const auto &TransformPlan =
          this->CurrentStreamAnalyzer->getTransformPlanByInst(NewStaticInst);
      bool NeedToHandleUseInformation = true;
      if (TransformPlan.Plan == StreamTransformPlan::PlanT::DELETE) {
        // Ignore delete instructions.
      } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STEP) {
        // Insert the step instructions.
        for (auto StepStream : TransformPlan.getStepStreams()) {
          this->CurrentStreamAnalyzer->getFuncSE()->step(StepStream,
                                                         this->Trace);

          // Create the new StepInst.
          auto StepInst = new StreamStepInst(StepStream);
          auto StepInstId = StepInst->getId();

          this->Trace->insertDynamicInst(NewInstIter, StepInst);

          /**
           * Handle the dependence for the step instruction.
           * Step inst should also wait for the dependent stream insts.
           */
          auto &RegDeps = this->Trace->RegDeps.at(StepInstId);
          // Add dependence to the previous me and register myself.
          auto StreamInst = StepStream->getInst();
          auto StreamInstIter = ActiveStreamInstMap.find(StreamInst);
          if (StreamInstIter == ActiveStreamInstMap.end()) {
            ActiveStreamInstMap.emplace(StreamInst, StepInstId);
          } else {
            RegDeps.emplace_back(nullptr, StreamInstIter->second);
            StreamInstIter->second = StepInstId;
          }

          this->StepInstCount++;
        }

        // Do not delete the original instruction.
      } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STORE) {
        // No need to handle store instructions.
      }

      /**
       * No need to handle user information for prefetch.
       */
    }
  }

  LLVM_DEBUG(llvm::errs() << "StreamPrefetch: Transform done.\n");
}
} // namespace

#undef DEBUG_TYPE

char StreamPrefetchPass::ID = 0;
static llvm::RegisterPass<StreamPrefetchPass>
    X("stream-prefetch-pass", "Stream prefetch transform pass", false, false);
