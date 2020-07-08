#include "StaticStreamRegionAnalyzer.h"

#include "Utils.h"

#define DEBUG_TYPE "StaticStreamRegionAnalyzer"

StaticStreamRegionAnalyzer::StaticStreamRegionAnalyzer(
    llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
    CachedLoopInfo *_CachedLI, CachedPostDominanceFrontier *_CachedPDF,
    CachedBBPredicateDataGraph *_CachedBBPredDG)
    : TopLoop(_TopLoop), DataLayout(_DataLayout), CachedLI(_CachedLI),
      CachedBBPredDG(_CachedBBPredDG),
      LI(_CachedLI->getLoopInfo(_TopLoop->getHeader()->getParent())),
      SE(_CachedLI->getScalarEvolution(_TopLoop->getHeader()->getParent())),
      PDT(_CachedPDF->getPostDominatorTree(
          _TopLoop->getHeader()->getParent())) {
  LLVM_DEBUG(llvm::dbgs() << "Constructing StaticStreamRegionAnalyzer for loop "
                          << LoopUtils::getLoopId(this->TopLoop) << '\n');
  this->initializeStreams();
  LLVM_DEBUG(llvm::dbgs() << "Initializing streams done.\n");
  this->markPredicateRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark predicate relationship done.\n");
  this->buildStreamAddrDepGraph();
  LLVM_DEBUG(llvm::dbgs() << "Building StreamAddrDepGraph done.\n");
  this->markAliasRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark alias relationship done.\n");
  this->buildStreamValueDepGraph();
  LLVM_DEBUG(llvm::dbgs() << "Building StreamValueDepGraph done.\n");
  // Update relationship must be after alias relationship.
  this->markUpdateRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark update relationship done.\n");
  this->analyzeIsCandidate();
  LLVM_DEBUG(llvm::dbgs() << "AnalyzeIsCandidate done.\n");
  this->markQualifiedStreams();
  LLVM_DEBUG(llvm::dbgs() << "Marking qualified streams done.\n");
  this->enforceBackEdgeDependence();
  LLVM_DEBUG(llvm::dbgs()
             << "Constructing StaticStreamRegionAnalyzer for loop done!\n");
}

StaticStreamRegionAnalyzer::~StaticStreamRegionAnalyzer() {
  for (auto &InstStreams : this->InstStaticStreamMap) {
    for (auto &Stream : InstStreams.second) {
      delete Stream;
      Stream = nullptr;
    }
    InstStreams.second.clear();
  }
  this->InstStaticStreamMap.clear();
}

void StaticStreamRegionAnalyzer::initializeStreams() {

  /**
   * Do a BFS to find all the IV streams in the header of both this loop and
   * subloops.
   */
  std::list<llvm::Loop *> Queue;
  Queue.emplace_back(this->TopLoop);
  while (!Queue.empty()) {
    auto CurrentLoop = Queue.front();
    Queue.pop_front();
    for (auto &SubLoop : CurrentLoop->getSubLoops()) {
      Queue.emplace_back(SubLoop);
    }
    auto HeaderBB = CurrentLoop->getHeader();
    auto PHINodes = HeaderBB->phis();
    for (auto PHINodeIter = PHINodes.begin(), PHINodeEnd = PHINodes.end();
         PHINodeIter != PHINodeEnd; ++PHINodeIter) {
      auto PHINode = &*PHINodeIter;
      this->initializeStreamForAllLoops(PHINode);
    }
  }

  /**
   * Find all the memory stream instructions.
   */
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if (!Utils::isMemAccessInst(StaticInst)) {
        continue;
      }
      this->initializeStreamForAllLoops(StaticInst);
    }
  }
}

void StaticStreamRegionAnalyzer::initializeStreamForAllLoops(
    llvm::Instruction *StreamInst) {
  auto InnerMostLoop = this->LI->getLoopFor(StreamInst->getParent());
  assert(InnerMostLoop != nullptr &&
         "Failed to get inner most loop for stream inst.");
  assert(this->TopLoop->contains(InnerMostLoop) &&
         "Stream inst is not within top loop.");

  auto &Streams =
      this->InstStaticStreamMap
          .emplace(std::piecewise_construct, std::forward_as_tuple(StreamInst),
                   std::forward_as_tuple())
          .first->second;
  assert(Streams.empty() &&
         "There is already streams initialized for the stream instruction.");

  auto ConfigureLoop = InnerMostLoop;
  do {
    auto LoopLevel =
        ConfigureLoop->getLoopDepth() - this->TopLoop->getLoopDepth();
    StaticStream *NewStream = nullptr;
    LLVM_DEBUG(llvm::dbgs()
               << "Initializing stream " << Utils::formatLLVMInst(StreamInst)
               << " Config Loop " << LoopUtils::getLoopId(ConfigureLoop)
               << '\n');
    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
      NewStream = new StaticIndVarStream(PHIInst, ConfigureLoop, InnerMostLoop,
                                         SE, PDT, DataLayout);
    } else {
      NewStream = new StaticMemStream(StreamInst, ConfigureLoop, InnerMostLoop,
                                      SE, PDT, DataLayout);
    }
    Streams.emplace_back(NewStream);

    ConfigureLoop = ConfigureLoop->getParentLoop();
  } while (this->TopLoop->contains(ConfigureLoop));
}

StaticStream *StaticStreamRegionAnalyzer::getStreamByInstAndConfigureLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfigureLoop) const {
  auto Iter = this->InstStaticStreamMap.find(Inst);
  if (Iter == this->InstStaticStreamMap.end()) {
    return nullptr;
  }
  const auto &Streams = Iter->second;
  for (const auto &S : Streams) {
    if (S->ConfigureLoop == ConfigureLoop) {
      return S;
    }
  }
  llvm_unreachable("Failed to find the stream at specified loop level.");
}

void StaticStreamRegionAnalyzer::markUpdateRelationship() {
  /**
   * Search for load-store update relationship.
   */
  for (auto &InstStreams : this->InstStaticStreamMap) {
    auto Inst = InstStreams.first;
    if (Inst->getOpcode() == llvm::Instruction::Load) {
      for (auto LoadSS : this->InstStaticStreamMap.at(Inst)) {
        this->markUpdateRelationshipForLoadStream(LoadSS);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::markUpdateRelationshipForLoadStream(
    StaticStream *LoadSS) {
  /**
   * Update relationship is a special case for value dependence.
   * 1. The StoreStream stores to the same address of the LoadStream, with same
   * type.
   * 2. There is only one such StoreStream.
   * 3. They are in the same BB.
   * TODO: We should check that there is no aliasing store between them.
   */
  LLVM_DEBUG(llvm::dbgs() << "== SSRA: Mark Update Relationship for "
                          << LoadSS->formatName() << '\n');

  auto LoadType = Utils::getMemElementType(LoadSS->Inst);
  auto AliasBaseSS = LoadSS->AliasBaseStream;
  if (!AliasBaseSS) {
    return;
  }
  StaticStream *CandidateSS = nullptr;
  for (auto AliasSS : AliasBaseSS->AliasedStreams) {
    LLVM_DEBUG(llvm::dbgs()
               << "==== Check AliasStream " << AliasSS->formatName() << '\n');
    if (AliasSS->AliasOffset != LoadSS->AliasOffset)
      continue;
    if (AliasSS->Inst->getOpcode() != llvm::Instruction::Store)
      continue;
    if (Utils::getMemElementType(AliasSS->Inst) != LoadType)
      continue;
    if (CandidateSS) {
      // Multiple candidates.
      return;
    } else {
      LLVM_DEBUG(llvm::dbgs()
                 << "==== Select candidate " << AliasSS->formatName() << '\n');
      CandidateSS = AliasSS;
    }
  }

  if (!CandidateSS) {
    return;
  }

  if (CandidateSS->Inst->getParent() != LoadSS->Inst->getParent()) {
    // Not same BB.
    return;
  }

  // Make sure we don't have multiple update relationship for same StoreSS.
  if (CandidateSS->UpdateStream) {
    return;
  }
  assert(!LoadSS->UpdateStream && "This LoadSS already has UpdateStream.");
  LLVM_DEBUG(llvm::dbgs() << "==== Set Update Stream "
                          << CandidateSS->formatName() << '\n');
  LoadSS->UpdateStream = CandidateSS;
  CandidateSS->UpdateStream = LoadSS;
}

void StaticStreamRegionAnalyzer::markPredicateRelationship() {
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {
      this->markPredicateRelationshipForLoopBB(Loop, BB);
    }
  }
}

void StaticStreamRegionAnalyzer::markPredicateRelationshipForLoopBB(
    const llvm::Loop *Loop, const llvm::BasicBlock *BB) {
  auto BBPredDG = this->CachedBBPredDG->getBBPredicateDataGraph(Loop, BB);
  if (!BBPredDG->isValid()) {
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid " << BB->getName() << '\n');
    return;
  }
  auto PredInputLoads = BBPredDG->getInputLoads();
  if (PredInputLoads.size() != 1) {
    // Complicate predicate is not supported.
    LLVM_DEBUG(llvm::dbgs()
               << "BBPredDG Multiple Load Inputs " << BB->getName() << '\n');
    return;
  }
  auto PredLoadInst = *(PredInputLoads.begin());
  auto PredLoadStream =
      this->getStreamByInstAndConfigureLoop(PredLoadInst, Loop);
  assert(PredLoadStream->BBPredDG == nullptr && "Multiple BBPredDG.");
  PredLoadStream->BBPredDG = BBPredDG;

  auto MarkStreamInBB = [this, PredLoadStream](const llvm::BasicBlock *TargetBB,
                                               bool PredicatedTrue) -> void {
    auto ConfigureLoop = PredLoadStream->ConfigureLoop;
    for (const auto &TargetInst : *TargetBB) {
      auto TargetStream =
          this->getStreamByInstAndConfigureLoop(&TargetInst, ConfigureLoop);
      if (!TargetStream) {
        continue;
      }
      LLVM_DEBUG(llvm::dbgs()
                 << "Add predicated relation " << PredLoadStream->formatName()
                 << " -- " << PredicatedTrue << " --> "
                 << TargetStream->formatName() << '\n');
      if (PredicatedTrue) {
        PredLoadStream->PredicatedTrueStreams.insert(TargetStream);
      } else {
        PredLoadStream->PredicatedFalseStreams.insert(TargetStream);
      }
    }
  };
  if (auto TrueBB = BBPredDG->getTrueBB()) {
    MarkStreamInBB(TrueBB, true);
  }
  if (auto FalseBB = BBPredDG->getFalseBB()) {
    MarkStreamInBB(FalseBB, false);
  }
}

void StaticStreamRegionAnalyzer::insertPredicateFuncInModule(
    std::unique_ptr<llvm::Module> &Module) {
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {

      auto BBPredDG = this->CachedBBPredDG->tryBBPredicateDataGraph(Loop, BB);
      if (!BBPredDG) {
        continue;
      }
      if (!BBPredDG->isValid()) {
        continue;
      }
      BBPredDG->generateFunction(BBPredDG->getFuncName(), Module);
    }
  }
}

void StaticStreamRegionAnalyzer::buildStreamAddrDepGraph() {
  auto GetStream = [this](const llvm::Instruction *Inst,
                          const llvm::Loop *ConfigureLoop) -> StaticStream * {
    return this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop);
  };
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      LLVM_DEBUG(llvm::dbgs()
                 << "== SSRA: Construct Graph for " << S->formatName() << '\n');
      S->constructGraph(GetStream);
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: constructGraph done.\n");
  /**
   * After add all the base streams, we are going to compute the base step root
   * streams. The computeBaseStepRootStreams() is by itself recursive. This will
   * result in some overhead, but hopefully the dependency chain is not very
   * long.
   */
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      LLVM_DEBUG(llvm::dbgs() << "== SSRA: ComputeBaseStepRoot for "
                              << S->formatName() << '\n');
      S->computeBaseStepRootStreams();
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: computeBaseStepRootStreams done.\n");
}

void StaticStreamRegionAnalyzer::markAliasRelationship() {
  /**
   * After construct basic stream dependence graph, we can analyze the coalesce
   * relationship. This is used when building LoadStreamDependenceGraph.
   */
  std::unordered_set<const llvm::Loop *> VisitedLoops;
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {
      if (!VisitedLoops.count(Loop)) {
        this->markAliasRelationshipForLoopBB(Loop);
        VisitedLoops.insert(Loop);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::markAliasRelationshipForLoopBB(
    const llvm::Loop *Loop) {
  /**
   * We mark aliased memory streams if:
   * 1. They share the same step root.
   * 2. They have scev.
   * 3. Their SCEV has a constant offset.
   */
  using StreamOffsetPair = std::pair<StaticStream *, int64_t>;
  std::list<std::vector<StreamOffsetPair>> CoalescedGroup;
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      auto S = this->getStreamByInstAndConfigureLoop(Inst, Loop);
      if (!S) {
        continue;
      }
      if (S->Type == StaticStream::TypeT::IV) {
        continue;
      }
      if (S->BaseStepRootStreams.size() != 1) {
        continue;
      }
      LLVM_DEBUG(llvm::dbgs() << "====== Try to coalesce stream: "
                              << S->formatName() << '\n');
      auto Addr = const_cast<llvm::Value *>(Utils::getMemAddrValue(S->Inst));
      auto AddrSCEV = this->SE->getSCEV(Addr);

      bool Coalesced = false;
      if (AddrSCEV) {
        LLVM_DEBUG(llvm::dbgs() << "AddrSCEV: "; AddrSCEV->dump());
        for (auto &Group : CoalescedGroup) {
          auto TargetS = Group.front().first;
          if (TargetS->BaseStepRootStreams != S->BaseStepRootStreams) {
            // Do not share the same step root.
            continue;
          }
          auto TargetAddr =
              const_cast<llvm::Value *>(Utils::getMemAddrValue(TargetS->Inst));
          auto TargetAddrSCEV = this->SE->getSCEV(TargetAddr);
          if (!TargetAddrSCEV) {
            continue;
          }
          // Check the scev.
          LLVM_DEBUG(llvm::dbgs()
                     << "TargetStream: " << TargetS->formatName() << '\n');
          LLVM_DEBUG(llvm::dbgs() << "TargetAddrSCEV: ";
                     TargetAddrSCEV->dump());
          auto MinusSCEV = this->SE->getMinusSCEV(AddrSCEV, TargetAddrSCEV);
          LLVM_DEBUG(llvm::dbgs() << "MinusSCEV: "; MinusSCEV->dump());
          auto OffsetSCEV = llvm::dyn_cast<llvm::SCEVConstant>(MinusSCEV);
          if (!OffsetSCEV) {
            // Not constant offset.
            continue;
          }
          LLVM_DEBUG(llvm::dbgs() << "OffsetSCEV: "; OffsetSCEV->dump());
          int64_t Offset = OffsetSCEV->getAPInt().getSExtValue();
          LLVM_DEBUG(llvm::dbgs()
                     << "Coalesced, offset: " << Offset
                     << " with stream: " << TargetS->formatName() << '\n');
          Coalesced = true;
          Group.emplace_back(S, Offset);
          break;
        }
      }

      if (!Coalesced) {
        LLVM_DEBUG(llvm::dbgs() << "New coalesce group\n");
        CoalescedGroup.emplace_back();
        CoalescedGroup.back().emplace_back(S, 0);
      }
    }
  }

  // Sort each group with increasing order of offset.
  for (auto &Group : CoalescedGroup) {
    std::sort(Group.begin(), Group.end(),
              [](const StreamOffsetPair &a, const StreamOffsetPair &b) -> bool {
                return a.second < b.second;
              });
    // Set the base/offset.
    auto BaseS = Group.front().first;
    int64_t BaseOffset = Group.front().second;
    for (auto &StreamOffset : Group) {
      auto S = StreamOffset.first;
      auto Offset = StreamOffset.second;
      // Remember the offset.
      S->AliasBaseStream = BaseS;
      S->AliasOffset = Offset - BaseOffset;
      BaseS->AliasedStreams.push_back(S);
    }
  }
}

void StaticStreamRegionAnalyzer::buildStreamValueDepGraph() {
  /**
   * After construct addr dependence graph, we can analyze the
   * ValueDG and Value dependence.
   * We handle AtomicRMW stream as a special store stream.
   */
  if (!StreamPassEnableValueDG) {
    return;
  }
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    if (Inst->getOpcode() == llvm::Instruction::Store ||
        Inst->getOpcode() == llvm::Instruction::AtomicRMW ||
        Inst->getOpcode() == llvm::Instruction::AtomicCmpXchg) {
      for (auto &S : InstStream.second) {
        this->buildValueDepForStoreOrAtomic(S);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::buildValueDepForStoreOrAtomic(
    StaticStream *StoreS) {

  // StoreS could be an Atomic stream.
  const auto StreamOpcode = StoreS->Inst->getOpcode();
  bool IsAtomic = (StreamOpcode == llvm::Instruction::AtomicRMW) ||
                  (StreamOpcode == llvm::Instruction::AtomicCmpXchg);

  if (StoreS->BaseStepRootStreams.size() != 1) {
    // Wierd stream has no step root stream. We do not try to analyze it.
    return;
  }
  /**
   * Let's try to construct the ValueDG.
   * So far we will first simply take every PHINode as an induction variable
   * and later enforce all the constraints.
   */
  auto IsIndVarStream = [](const llvm::PHINode *PHI) -> bool { return true; };
  llvm::Value *StoreValue = nullptr;
  switch (StreamOpcode) {
  default:
    llvm_unreachable("Invalid StoreStream Opcode.");
  case llvm::Instruction::Store:
    StoreValue = StoreS->Inst->getOperand(0);
    break;
  case llvm::Instruction::AtomicRMW:
    StoreValue = StoreS->Inst->getOperand(1);
    break;
  case llvm::Instruction::AtomicCmpXchg:
    StoreValue = StoreS->Inst->getOperand(2);
    break;
  }
  auto ValueDG = std::make_unique<StreamDataGraph>(StoreS->ConfigureLoop,
                                                   StoreValue, IsIndVarStream);
  /**
   * Enforce all the constraints.
   * 1. ValueDG should have no circle.
   * 2. ValueDG should only have Load/LoopInvariant inputs, at most 4 inputs.
   * 3. All the load inputs should be within the same BB of this store.
   * 4. All the load and the store should have the same step root stream.
   * 5. If multiple loads, they must be coalesced and continuous.
   */
  if (ValueDG->hasCircle()) {
    return;
  }
  if (ValueDG->hasPHINodeInComputeInsts() ||
      ValueDG->hasCallInstInComputeInsts()) {
    return;
  }
  if (ValueDG->getInputs().size() > 4) {
    return;
  }
  std::unordered_set<const llvm::LoadInst *> LoadInputs;
  for (auto Input : ValueDG->getInputs()) {
    if (auto InputInst = llvm::dyn_cast<llvm::Instruction>(Input)) {
      if (auto LoadInput = llvm::dyn_cast<llvm::LoadInst>(InputInst)) {
        // This is a LoadInput.
        LoadInputs.insert(LoadInput);
      } else if (StoreS->ConfigureLoop->contains(InputInst)) {
        // Found LoopVariant input.
        return;
      }
    }
  }
  StaticStream::StreamVec LoadInputStreams;
  for (auto LoadInput : LoadInputs) {
    if (LoadInput->getParent() != StoreS->Inst->getParent()) {
      // Not same BB.
      return;
    }
    auto LoadS =
        this->getStreamByInstAndConfigureLoop(LoadInput, StoreS->ConfigureLoop);
    if (LoadS->BaseStepRootStreams != StoreS->BaseStepRootStreams) {
      // They are not stepped by the same stream.
      return;
    }
    LoadInputStreams.push_back(LoadS);
  }
  // Check that all load inputs are continuously coalesced.
  if (!LoadInputStreams.empty()) {
    auto LoadInputAliasBaseStream = LoadInputStreams.front()->AliasBaseStream;
    assert(LoadInputAliasBaseStream && "Missing AliasBaseStream.");
    for (auto LoadS : LoadInputStreams) {
      if (LoadS->AliasBaseStream != LoadInputAliasBaseStream) {
        // Mismatch in the AliasBaseStream.
        return;
      }
    }
    // Sort the LoadInputStreams with AliasOffset.
    std::sort(LoadInputStreams.begin(), LoadInputStreams.end(),
              [](const StaticStream *SA, const StaticStream *SB) -> bool {
                return SA->AliasOffset < SB->AliasOffset;
              });
    // Check that these input streams are continuous.
    for (int Idx = 1, End = LoadInputStreams.size(); Idx < End; ++Idx) {
      auto PrevS = LoadInputStreams.at(Idx - 1);
      auto PrevOffset = PrevS->AliasOffset;
      auto PrevMemElementSize = PrevS->getMemElementSize();
      auto CurOffset = LoadInputStreams.at(Idx)->AliasOffset;
      if (PrevOffset + PrevMemElementSize < CurOffset) {
        // These are not continuous.
        return;
      }
    }
  }
  /**
   * We passed all checks, we remember this relationship.
   */
  for (auto LoadS : LoadInputStreams) {
    LoadS->LoadStoreDepStreams.insert(StoreS);
    StoreS->LoadStoreBaseStreams.insert(LoadS);
  }
  /**
   * Extend the ValueDG with the final atomic operation.
   */
  if (IsAtomic) {
    ValueDG->extendTailAtomicInst(StoreS->Inst, StoreS->FusedLoadOps);
  }
  StoreS->ValueDG = std::move(ValueDG);
}

void StaticStreamRegionAnalyzer::analyzeIsCandidate() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->analyzeIsCandidate();
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "analyzeIsCandidate done.\n");
}

void StaticStreamRegionAnalyzer::markQualifiedStreams() {
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: MarkQualifiedStreams\n");
  std::list<StaticStream *> Queue;
  // First stage: ignore back-edge dependence.
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      auto IsQualified = S->checkIsQualifiedWithoutBackEdgeDep();
      if (IsQualified) {
        Queue.emplace_back(S);
      }
    }
  }

  /**
   * BFS on the qualified streams to propagate the qualified signal.
   */
  while (!Queue.empty()) {
    auto S = Queue.front();
    Queue.pop_front();
    if (S->isQualified()) {
      // We have already processed this stream.
      continue;
    }
    if (!S->checkIsQualifiedWithoutBackEdgeDep()) {
      assert(false && "Stream should be qualified to be inserted into the "
                      "qualifying queue.");
    }
    LLVM_DEBUG(llvm::dbgs() << "Mark Qualified " << S->formatName() << '\n');
    S->setIsQualified(true);
    // Check all the dependent streams.
    for (const auto &DependentStream : S->DependentStreams) {
      if (DependentStream->checkIsQualifiedWithoutBackEdgeDep()) {
        Queue.emplace_back(DependentStream);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::enforceBackEdgeDependence() {
  /**
   * For IVStreams with back edge loads, we assume they are qualified from the
   * beginning point. Now it's time to check if their base memory streams are
   * actually qualified. If not, we need to propagate the dequalified signal
   * along the dependence chain.
   */
  std::list<StaticStream *> DisqualifiedQueue;
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        if (!S->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(S);
        }
      }
    }
  }

  /**
   * Propagate the disqualfied signal.
   */
  while (!DisqualifiedQueue.empty()) {
    auto S = DisqualifiedQueue.front();
    DisqualifiedQueue.pop_front();
    if (!S->isQualified()) {
      // This stream is already disqualified.
      continue;
    }
    S->setIsQualified(false);

    // Propagate to dependent streams.
    for (const auto &DependentStream : S->DependentStreams) {
      if (DependentStream->isQualified()) {
        if (!DependentStream->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(DependentStream);
        }
      }
    }

    // Also need to check back edges.
    for (const auto &DependentStream : S->BackIVDependentStreams) {
      if (DependentStream->isQualified()) {
        if (!DependentStream->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(DependentStream);
        }
      }
    }
  }
}