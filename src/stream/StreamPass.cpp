#include "stream/StreamPass.h"

#include "llvm/Support/FileSystem.h"

#include <iomanip>
#include <sstream>

#define DEBUG_TYPE "StreamPass"

llvm::cl::opt<StreamPassChooseStrategyE> StreamPassChooseStrategy(
    "stream-pass-choose-strategy",
    llvm::cl::desc("Choose how to choose the configure loop level:"),
    llvm::cl::values(clEnumValN(StreamPassChooseStrategyE::OUTER_MOST, "outer",
                                "Always pick the outer most loop level."),
                     clEnumValN(StreamPassChooseStrategyE::INNER_MOST, "inner",
                                "Always pick the inner most loop level.")));

bool StreamPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);
  this->MemorizedStreamInst.clear();
  return Ret;
}

bool StreamPass::finalize(llvm::Module &Module) {
  return ReplayTrace::finalize(Module);
}

void StreamPass::dumpStats(std::ostream &O) {

  O << "--------------- Stats -----------------\n";
#define print(value) (O << #value << ": " << this->value << '\n')
  print(DynInstCount);
  print(DynMemInstCount);
  print(StepInstCount);
  print(DeletedInstCount);
  print(ConfigInstCount);
#undef print
  for (const auto &LoopStreamAnalyzer : this->LoopStreamAnalyzerMap) {
    LoopStreamAnalyzer.second->dumpStats();
  }
}

StreamRegionAnalyzer *
StreamPass::getAnalyzerByLoop(const llvm::Loop *Loop) const {
  if (this->LoopStreamAnalyzerMap.count(Loop) == 0) {
    return nullptr;
  }
  return this->LoopStreamAnalyzerMap.at(Loop).get();
}

void StreamPass::transform() {
  this->analyzeStream();

  delete this->Trace;
  this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  this->transformStream();

  // End all for every region.
  for (auto &LoopStreamAnalyzer : this->LoopStreamAnalyzerMap) {
    auto &Analyzer = LoopStreamAnalyzer.second;
    Analyzer->endTransform();
  }

  // /**
  //  * Finalize the infomations, after we have the coalesce information.
  //  */
  // for (auto &InstStreamEntry : this->InstStreamMap) {
  //   for (auto &S : InstStreamEntry.second) {
  //     S->finalizeInfo(this->Trace->DataLayout);
  //   }
  // }

  // /**
  //  * Finally dump the information for loops.
  //  */
  // for (const auto &LoopStreamsPair : this->ChosenLoopSortedStreams) {
  //   std::string LoopInfoFileName = this->OutputExtraFolderPath + "/" +
  //                                  LoopUtils::getLoopId(LoopStreamsPair.first)
  //                                  +
  //                                  ".info.txt";
  //   std::ofstream LoopInfoOS(LoopInfoFileName);
  //   assert(LoopInfoOS.is_open() && "Failed to open a loop info file.");
  //   this->dumpInfoForLoop(LoopStreamsPair.first, LoopInfoOS, "");
  // }
}

void StreamPass::pushLoopStack(LoopStackT &LoopStack, llvm::Loop *Loop) {
  if (LoopStack.empty()) {
    // Get or create the region analyzer.
    if (this->LoopStreamAnalyzerMap.count(Loop) == 0) {
      auto StreamAnalyzer = std::make_unique<StreamRegionAnalyzer>(
          this->RegionIdx, Loop,
          this->CachedLI->getLoopInfo(Loop->getHeader()->getParent()),
          this->Trace->DataLayout, this->OutputExtraFolderPath);
      this->RegionIdx++;
      this->LoopStreamAnalyzerMap.insert(
          std::make_pair(Loop, std::move(StreamAnalyzer)));
    }
    this->CurrentStreamAnalyzer = this->LoopStreamAnalyzerMap.at(Loop).get();
  }

  LoopStack.emplace_back(Loop);
}

void StreamPass::addAccess(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  {
    // Initialize the global count.
    auto Iter = this->MemAccessInstCount.find(StaticInst);
    if (Iter == this->MemAccessInstCount.end()) {
      this->MemAccessInstCount.emplace(StaticInst, 0);
    }
  }
  this->MemAccessInstCount.at(StaticInst)++;
}

void StreamPass::popLoopStack(LoopStackT &LoopStack) {
  assert(!LoopStack.empty() && "Empty loop stack when popLoopStack is called.");

  // Deactivate all the streams in this loop level.
  const auto &EndedLoop = LoopStack.back();

  this->CurrentStreamAnalyzer->endIter(LoopStack.back());
  this->CurrentStreamAnalyzer->endLoop(LoopStack.back());

  LoopStack.pop_back();
}

bool StreamPass::isLoopContinuous(const llvm::Loop *Loop) {
  auto Iter = this->MemorizedLoopContinuous.find(Loop);
  if (Iter == this->MemorizedLoopContinuous.end()) {
    Iter = this->MemorizedLoopContinuous
               .emplace(Loop, LoopUtils::isLoopContinuous(Loop))
               .first;
  }
  return Iter->second;
}

void StreamPass::analyzeStream() {

  DEBUG(llvm::errs() << "Stream: Start analysis.\n");

  LoopStackT LoopStack;

  this->RegionIdx = 0;

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    DynamicInstruction *NewDynamicInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeadOfCandidate = false;
    bool IsMemAccess = false;

    while (this->Trace->DynamicInstructionList.size() > 100000) {
      this->Trace->commitOneDynamicInst();
    }

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewDynamicInst = *NewInstIter;
      NewStaticInst = NewDynamicInst->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

      this->DynInstCount++;

      IsMemAccess = Utils::isMemAccessInst(NewStaticInst);

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        IsAtHeadOfCandidate =
            LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst) &&
            this->isLoopContinuous(NewLoop);
      }
    } else {
      // This is the end.
      // We should end all the active streams.
      while (!LoopStack.empty()) {
        this->popLoopStack(LoopStack);
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Trace->commitOneDynamicInst();
      }

      /**
       * Notify all region analyzer to wrap up.
       */
      for (auto &LoopStreamAnalyzer : this->LoopStreamAnalyzerMap) {
        LoopStreamAnalyzer.second->endRegion(StreamPassChooseStrategy);
      }

      break;
    }

    while (!LoopStack.empty()) {
      if (LoopStack.back()->contains(NewStaticInst)) {
        break;
      }
      this->popLoopStack(LoopStack);
    }

    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back() != NewLoop) {
        // A new loop.
        // See if we are first time in a region.
        this->pushLoopStack(LoopStack, NewLoop);
      } else {
        // This means that we are at a new iteration.
        this->CurrentStreamAnalyzer->endIter(LoopStack.back());
      }
    }

    if (IsMemAccess) {
      this->DynMemInstCount++;
      this->addAccess(NewDynamicInst);

      if (!LoopStack.empty()) {
        // We are in a region.
        this->CurrentStreamAnalyzer->addMemAccess(NewDynamicInst, this->Trace);
      }

      this->CacheWarmerPtr->addAccess(Utils::getMemAddr(NewDynamicInst));
    }

    /**
     * Handle the value for induction variable streams.
     */
    if (!LoopStack.empty()) {
      this->CurrentStreamAnalyzer->addIVAccess(NewDynamicInst);
    }
  }
}

void StreamPass::pushLoopStackAndConfigureStreams(
    LoopStackT &LoopStack, llvm::Loop *NewLoop,
    DataGraph::DynamicInstIter NewInstIter,
    ActiveStreamInstMapT &ActiveStreamInstMap) {

  if (LoopStack.empty()) {
    // We entering a new region.
    assert(this->LoopStreamAnalyzerMap.count(NewLoop) != 0 &&
           "Missing analyzer for transforming loops.");
    this->CurrentStreamAnalyzer = this->LoopStreamAnalyzerMap.at(NewLoop).get();
    DEBUG(llvm::errs() << "Enter Region " << LoopUtils::getLoopId(NewLoop)
                       << '\n');
  }

  LoopStack.emplace_back(NewLoop);

  auto SortedStreams =
      this->CurrentStreamAnalyzer->getSortedChosenStreamsByConfiguredLoop(
          NewLoop);

  auto NewDynamicInst = *NewInstIter;

  std::unordered_set<DynamicInstruction::DynamicId> InitDepIds;
  for (const auto &RegDep : this->Trace->RegDeps.at(NewDynamicInst->getId())) {
    InitDepIds.insert(RegDep.second);
  }

  for (auto &S : SortedStreams) {

    // Inform the stream engine.
    DEBUG(llvm::errs() << "Configure stream " << S->formatName() << '\n');
    this->CurrentStreamAnalyzer->getFuncSE()->configure(S, this->Trace);

    auto Inst = S->getInst();
    auto ConfigInst = new StreamConfigInst(S);
    auto ConfigInstId = ConfigInst->getId();

    this->Trace->insertDynamicInst(NewInstIter, ConfigInst);

    /**
     * Insert the register dependence.
     */
    auto &RegDeps = this->Trace->RegDeps.at(ConfigInstId);
    for (auto &BaseStream : S->getBaseStreams()) {
      auto BaseStreamInst = BaseStream->getInst();
      auto ActiveStreamInstMapIter = ActiveStreamInstMap.find(BaseStreamInst);
      if (ActiveStreamInstMapIter != ActiveStreamInstMap.end()) {
        RegDeps.emplace_back(nullptr, ActiveStreamInstMapIter->second);
      }
    }

    /**
     * Also be dependent on previous step/config inst of myself.
     * Also add myself in the active stream inst map.
     */
    auto ActiveStreamInstMapIter = ActiveStreamInstMap.find(Inst);
    if (ActiveStreamInstMapIter != ActiveStreamInstMap.end()) {
      // There is a previous dependence.
      RegDeps.emplace_back(nullptr, ActiveStreamInstMapIter->second);
      ActiveStreamInstMapIter->second = ConfigInstId;
    } else {
      ActiveStreamInstMap.emplace(Inst, ConfigInstId);
    }

    /**
     * Also insert the init deps.
     */
    for (auto DepId : InitDepIds) {
      RegDeps.emplace_back(nullptr, DepId);
    }

    this->ConfigInstCount++;
  }
}

void StreamPass::popLoopStackAndUnconfigureStreams(
    LoopStackT &LoopStack, DataGraph::DynamicInstIter NewInstIter,
    ActiveStreamInstMapT &ActiveStreamInstMap) {
  assert(!LoopStack.empty() &&
         "Loop stack is empty when calling popLoopStackAndUnconfigureStreams.");
  auto EndedLoop = LoopStack.back();

  std::unordered_set<DynamicInstruction::DynamicId> InitDepIds;

  if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
    auto NewDynamicInst = *NewInstIter;
    for (const auto &RegDep :
         this->Trace->RegDeps.at(NewDynamicInst->getId())) {
      InitDepIds.insert(RegDep.second);
    }
  }

  auto SortedStreams =
      this->CurrentStreamAnalyzer->getSortedChosenStreamsByConfiguredLoop(
          EndedLoop);

  // Unconfigure in reverse order.
  for (auto StreamIter = SortedStreams.rbegin(),
            StreamEnd = SortedStreams.rend();
       StreamIter != StreamEnd; ++StreamIter) {
    auto S = *StreamIter;
    auto Inst = S->getInst();

    auto EndInst = new StreamEndInst(S);
    auto EndInstId = EndInst->getId();

    this->Trace->insertDynamicInst(NewInstIter, EndInst);

    /**
     * Insert the register dependence.
     */
    auto &RegDeps = this->Trace->RegDeps.at(EndInstId);
    for (auto &BaseStream : S->getBaseStreams()) {
      auto BaseStreamInst = BaseStream->getInst();
      auto ActiveStreamInstMapIter = ActiveStreamInstMap.find(BaseStreamInst);
      if (ActiveStreamInstMapIter != ActiveStreamInstMap.end()) {
        RegDeps.emplace_back(nullptr, ActiveStreamInstMapIter->second);
      }
    }

    /**
     * Also be dependent on previous step/config inst of myself.
     * Also add myself in the active stream inst map.
     */
    auto ActiveStreamInstMapIter = ActiveStreamInstMap.find(Inst);
    assert(ActiveStreamInstMapIter != ActiveStreamInstMap.end() &&
           "The stream is not configured.");
    // ActiveStreamInstMap.erase(ActiveStreamInstMapIter);
    if (ActiveStreamInstMapIter != ActiveStreamInstMap.end()) {
      // There is a previous dependence.
      RegDeps.emplace_back(nullptr, ActiveStreamInstMapIter->second);
      ActiveStreamInstMapIter->second = EndInstId;
    } else {
      ActiveStreamInstMap.emplace(Inst, EndInstId);
    }

    /**
     * Also insert the init deps.
     */
    for (auto DepId : InitDepIds) {
      RegDeps.emplace_back(nullptr, DepId);
    }

    /**
     * Inform the stream engine.
     */
    this->CurrentStreamAnalyzer->getFuncSE()->endStream(S);
  }

  LoopStack.pop_back();
  if (LoopStack.empty()) {
    this->CurrentStreamAnalyzer = nullptr;
  }
}

void StreamPass::DEBUG_TRANSFORMED_STREAM(DynamicInstruction *DynamicInst) {
  DEBUG(llvm::errs() << DynamicInst->getId() << ' ' << DynamicInst->getOpName()
                     << ' ');
  if (auto StaticInst = DynamicInst->getStaticInstruction()) {
    DEBUG(llvm::errs() << LoopUtils::formatLLVMInst(StaticInst));
  }
  DEBUG(llvm::errs() << " reg-deps ");
  for (const auto &RegDep : this->Trace->RegDeps.at(DynamicInst->getId())) {
    DEBUG(llvm::errs() << RegDep.second << ' ');
  }
  DEBUG(llvm::errs() << '\n');
}

void StreamPass::transformStream() {
  DEBUG(llvm::errs() << "Stream: Start transform.\n");

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
      // if (DynamicInst->getId() > 27400 && DynamicInst->getId() < 27600) {
      //   DEBUG(this->DEBUG_TRANSFORMED_STREAM(DynamicInst));
      // }

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
        /**
         * This is important to make the future instruction not dependent on
         * this deleted instruction.
         */
        this->Trace->DynamicFrameStack.front()
            .updateRegisterDependenceLookUpMap(NewStaticInst,
                                               std::list<DynamicId>());

        auto NewDynamicId = NewDynamicInst->getId();
        this->Trace->commitDynamicInst(NewDynamicId);
        this->Trace->DynamicInstructionList.erase(NewInstIter);
        NewDynamicInst = nullptr;
        /**
         * No more handling use information for the deleted instruction.
         */
        this->DeletedInstCount++;
        NeedToHandleUseInformation = false;

      } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STEP) {

        /**
         * A step transform plan means that this instructions is deleted, and
         * one or more step instructions are inserted.
         */

        // Insert all the step instructions.
        for (auto StepStream : TransformPlan.getStepStreams()) {
          // Inform the functional stream engine that this stream stepped.
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

        // Clear the possible future dependence on the original static
        // instruction.
        this->Trace->DynamicFrameStack.front()
            .updateRegisterDependenceLookUpMap(NewStaticInst,
                                               std::list<DynamicId>());

        // Delete this instruction.
        auto NewDynamicId = NewDynamicInst->getId();
        this->Trace->commitDynamicInst(NewDynamicId);
        this->Trace->DynamicInstructionList.erase(NewInstIter);
        NewDynamicInst = nullptr;
        /**
         * No more handling use information for the deleted instruction.
         */
        this->DeletedInstCount++;
        NeedToHandleUseInformation = false;

      } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STORE) {

        assert(llvm::isa<llvm::StoreInst>(NewStaticInst) &&
               "STORE plan for non store instruction.");

        this->Trace->DynamicFrameStack.front()
            .updateRegisterDependenceLookUpMap(NewStaticInst,
                                               std::list<DynamicId>());

        /**
         * REPLACE the original store instruction with our special stream
         * store.
         */
        auto NewDynamicId = NewDynamicInst->getId();
        delete NewDynamicInst;

        auto StoreInst =
            new StreamStoreInst(TransformPlan.getParamStream(), NewDynamicId);
        *NewInstIter = StoreInst;
        NewDynamicInst = StoreInst;

        /**
         * Remove register dependences for the stored address.
         */
        auto &RegDeps = this->Trace->RegDeps.at(NewDynamicId);
        auto RegDepIter = RegDeps.begin();
        while (RegDepIter != RegDeps.end()) {
          if (RegDepIter->first != nullptr &&
              RegDepIter->first == NewStaticInst->getOperand(1)) {
            // This is a register dependence on the address.
            RegDepIter = RegDeps.erase(RegDepIter);
          } else {
            // This is a register dependence on the value.
            ++RegDepIter;
          }
        }

        /**
         * Stream store is guaranteed to be no alias we have to worry.
         */
        auto &MemDeps = this->Trace->MemDeps.at(NewDynamicId);
        MemDeps.clear();

        /**
         * If the store stream has not step instruction, we have to explicitly
         * serialize between these stream-store instructions.
         */
        auto StoreStream = TransformPlan.getParamStream();
        auto StoreInstId = NewDynamicId;
        if (StoreStream->getBaseStepRootStreams().empty()) {
          // There is no step stream for me.

          /**
           * Handle the use information.
           */
          auto &RegDeps = this->Trace->RegDeps.at(NewDynamicInst->getId());
          for (auto &UsedStream : TransformPlan.getUsedStreams()) {
            // Add the used stream id to the dynamic instruction.
            NewDynamicInst->addUsedStreamId(UsedStream->getStreamId());
            auto UsedStreamInst = UsedStream->getInst();
            // Inform the used stream that the current entry is used.
            this->CurrentStreamAnalyzer->getFuncSE()->access(UsedStream);
            auto UsedStreamInstIter = ActiveStreamInstMap.find(UsedStreamInst);
            if (UsedStreamInstIter != ActiveStreamInstMap.end()) {
              RegDeps.emplace_back(nullptr, UsedStreamInstIter->second);
            }
          }

          // We already handle the use information.
          NeedToHandleUseInformation = false;

          // Add dependence to the previous me and register myself.
          auto StreamInst = StoreStream->getInst();
          auto StreamInstIter = ActiveStreamInstMap.find(StreamInst);
          if (StreamInstIter == ActiveStreamInstMap.end()) {
            ActiveStreamInstMap.emplace(StreamInst, NewDynamicId);
          } else {
            RegDeps.emplace_back(nullptr, StreamInstIter->second);
            StreamInstIter->second = NewDynamicId;
          }
        }
      }

      if (NeedToHandleUseInformation) {
        /**
         * Handle the use information.
         */
        auto &RegDeps = this->Trace->RegDeps.at(NewDynamicInst->getId());
        for (auto &UsedStream : TransformPlan.getUsedStreams()) {
          // Add the used stream id to the dynamic instruction.
          NewDynamicInst->addUsedStreamId(UsedStream->getStreamId());
          auto UsedStreamInst = UsedStream->getInst();
          // Inform the used stream that the current entry is used.
          this->CurrentStreamAnalyzer->getFuncSE()->access(UsedStream);
        }
      }
    }
// Debug a certain tranformed loop.
#define DEBUG_LOOP_TRANSFORMED "train::bb13"
    for (auto &Loop : LoopStack) {
      if (LoopUtils::getLoopId(Loop) == DEBUG_LOOP_TRANSFORMED) {
        if (NewDynamicInst != nullptr) {
          DEBUG(this->DEBUG_TRANSFORMED_STREAM(NewDynamicInst));
        }
        break;
      }
    }
#undef DEBUG_LOOP_TRANSFORMED
  }

  DEBUG(llvm::errs() << "Stream: Transform done.\n");
}

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);
