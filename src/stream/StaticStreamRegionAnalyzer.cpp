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
  LLVM_DEBUG(llvm::errs() << "Constructing StaticStreamRegionAnalyzer for loop "
                          << LoopUtils::getLoopId(this->TopLoop) << '\n');
  this->initializeStreams();
  LLVM_DEBUG(llvm::errs() << "Initializing streams done.\n");
  this->markUpdateRelationship();
  LLVM_DEBUG(llvm::errs() << "Mark update relationship done.\n");
  this->markPredicateRelationship();
  LLVM_DEBUG(llvm::errs() << "Mark predicate relationship done.\n");
  this->buildStreamDependenceGraph();
  LLVM_DEBUG(llvm::errs() << "Building stream dependence graph done.\n");
  this->markQualifiedStreams();
  LLVM_DEBUG(llvm::errs() << "Marking qualified streams done.\n");
  this->enforceBackEdgeDependence();
  LLVM_DEBUG(llvm::errs()
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
    LLVM_DEBUG(llvm::errs()
               << "Initializing stream " << Utils::formatLLVMInst(StreamInst)
               << " Config Loop " << LoopUtils::getLoopId(ConfigureLoop)
               << '\n');
    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
      NewStream = new StaticIndVarStream(PHIInst, ConfigureLoop, InnerMostLoop,
                                         SE, PDT);
    } else {
      NewStream = new StaticMemStream(StreamInst, ConfigureLoop, InnerMostLoop,
                                      SE, PDT);
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
    if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(Inst)) {
      this->markUpdateRelationshipForStore(StoreInst);
    }
  }
}

void StaticStreamRegionAnalyzer::markUpdateRelationshipForStore(
    const llvm::StoreInst *StoreInst) {
  /**
   * So far we just search for load in the same basic block and before me.
   */
  LLVM_DEBUG(llvm::errs() << "Search update stream for "
                          << Utils::formatLLVMInst(StoreInst) << '\n');
  auto BB = StoreInst->getParent();
  auto StoreAddrValue = Utils::getMemAddrValue(StoreInst);
  auto StoreValue = StoreInst->getOperand(0);
  auto StoreValueSCEV = this->SE->getSCEV(StoreValue);
  for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
       ++InstIter) {
    auto Inst = &*InstIter;
    if (Inst == StoreInst) {
      // We have encounter ourself.
      break;
    }
    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
      auto LoadAddrValue = Utils::getMemAddrValue(LoadInst);
      // So far we just check for the same address.
      if (StoreAddrValue == LoadAddrValue) {
        // Found update relationship.
        LLVM_DEBUG(llvm::errs() << "Found update load "
                                << Utils::formatLLVMInst(LoadInst) << '\n');
        for (auto StoreStream : this->InstStaticStreamMap.at(StoreInst)) {
          auto ConfigureLoop = StoreStream->ConfigureLoop;
          auto LoadStream =
              this->getStreamByInstAndConfigureLoop(LoadInst, ConfigureLoop);
          LLVM_DEBUG(llvm::errs() << "Update for LoadStream "
                                  << LoadStream->formatName() << '\n');
          assert(LoadStream->UpdateStream == nullptr &&
                 "More than one update stream.");
          assert(StoreStream->UpdateStream == nullptr &&
                 "More than one update stream.");
          LoadStream->UpdateStream = StoreStream;
          LoadStream->StaticStreamInfo.set_has_update(true);
          StoreStream->UpdateStream = LoadStream;
          StoreStream->StaticStreamInfo.set_has_update(true);
          auto IsConstantStore =
              this->SE->isLoopInvariant(StoreValueSCEV, ConfigureLoop);
          StoreStream->StaticStreamInfo.set_has_const_update(IsConstantStore);
          LoadStream->StaticStreamInfo.set_has_const_update(IsConstantStore);
        }
      }
    }
  }
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

void StaticStreamRegionAnalyzer::buildStreamDependenceGraph() {
  auto GetStream = [this](const llvm::Instruction *Inst,
                          const llvm::Loop *ConfigureLoop) -> StaticStream * {
    return this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop);
  };
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      LLVM_DEBUG(llvm::errs()
                 << "Construct Graph for " << S->formatName() << '\n');
      S->constructGraph(GetStream);
    }
  }
  LLVM_DEBUG(llvm::errs() << "constructGraph done.\n");
  /**
   * After add all the base streams, we are going to compute the base step root
   * streams. The computeBaseStepRootStreams() is by itself recursive. This will
   * result in some overhead, but hopefully the dependency chain is not very
   * long.
   */
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->computeBaseStepRootStreams();
    }
  }
  LLVM_DEBUG(llvm::errs() << "computeBaseStepRootStreams done.\n");
  /**
   * After construct step root streams, we can analyze if the stream is a
   * candidate.
   */
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->analyzeIsCandidate();
    }
  }
  LLVM_DEBUG(llvm::errs() << "analyzeIsCandidate done.\n");
}

void StaticStreamRegionAnalyzer::markQualifiedStreams() {
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