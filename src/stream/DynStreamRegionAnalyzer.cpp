#include "DynStreamRegionAnalyzer.h"

#include <iomanip>
#include <queue>
#include <sstream>

#define DEBUG_TYPE "StreamRegionAnalyzer"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

DynStreamRegionAnalyzer::DynStreamRegionAnalyzer(
    llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
    CachedLoopInfo *_CachedLI, CachedPostDominanceFrontier *_CachedPDF,
    CachedBBBranchDataGraph *_CachedBBBranchDG, uint64_t _RegionIdx,
    const std::string &_RootPath)
    : StaticStreamRegionAnalyzer(_TopLoop, _DataLayout, _CachedLI, _CachedPDF,
                                 _CachedBBBranchDG, _RegionIdx, _RootPath) {
  this->initializeStreams();
}

DynStreamRegionAnalyzer::~DynStreamRegionAnalyzer() {
  for (auto &StaticDynStream : this->StaticDynStreamMap) {
    delete StaticDynStream.second;
    StaticDynStream.second = nullptr;
  }
  this->StaticDynStreamMap.clear();
}

void DynStreamRegionAnalyzer::initializeStreams() {
  /**
   * Use the static streams as a template to initialize the dynamic streams.
   */
  auto IsIVStream = [this](const llvm::PHINode *PHINode) -> bool {
    return this->InstStaticStreamMap.count(PHINode) != 0;
  };
  for (auto &InstStaticStream : this->InstStaticStreamMap) {
    auto StreamInst = InstStaticStream.first;

    for (auto &StaticStream : InstStaticStream.second) {
      if (!this->TopLoop->contains(StaticStream->ConfigureLoop)) {
        break;
      }
      DynStream *NewStream = nullptr;
      if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
        NewStream = new DynIndVarStream(this->getAnalyzePath(),
                                        this->getAnalyzeRelativePath(),
                                        StaticStream, this->DataLayout);
      } else {
        NewStream = new DynMemStream(
            this->getAnalyzePath(), this->getAnalyzeRelativePath(),
            StaticStream, this->DataLayout, IsIVStream);
      }
      this->StaticDynStreamMap.emplace(StaticStream, NewStream);
    }
  }
}

void DynStreamRegionAnalyzer::addMemAccess(DynamicInstruction *DynamicInst,
                                           DataGraph *DG) {
  this->numDynamicMemAccesses++;
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  assert(Utils::isMemAccessInst(StaticInst) &&
         "Should be memory access instruction.");
  auto Iter = this->InstStaticStreamMap.find(StaticInst);
  assert(Iter != this->InstStaticStreamMap.end() &&
         "Failed to find the stream.");
  uint64_t Addr = Utils::getMemAddr(DynamicInst);
  for (auto &SS : Iter->second) {
    auto DynS = this->getDynStreamByStaticStream(SS);
    DynS->addAccess(Addr, DynamicInst->getId());
  }

  // Handle the aliasing.
  auto MemDepsIter = DG->MemDeps.find(DynamicInst->getId());
  if (MemDepsIter == DG->MemDeps.end()) {
    // There is no known alias.
    return;
  }
  for (const auto &MemDepId : MemDepsIter->second) {
    // Check if this dynamic instruction is still alive.
    auto MemDepDynamicInst = DG->getAliveDynamicInst(MemDepId);
    if (MemDepDynamicInst == nullptr) {
      // This dynamic instruction is already committed. We assume that the
      // dependence is old enough to be safely ignored.
      continue;
    }
    auto MemDepStaticInst = MemDepDynamicInst->getStaticInstruction();
    assert(MemDepStaticInst != nullptr &&
           "Invalid memory dependent llvm static instruction.");

    for (auto &S : Iter->second) {
      auto MStream = reinterpret_cast<DynMemStream *>(S);
      const auto ConfigureLoop = MStream->getLoop();
      const auto StartId = MStream->getStartId();
      if (StartId == DynamicInstruction::InvalidId) {
        // This stream is not even active (or more precisely, has not seen the
        // first access).
        continue;
      }
      // Ignore those instruction not from our configuration loop.
      if (!ConfigureLoop->contains(MemDepStaticInst)) {
        continue;
      }
      // Check if they are older than the start id.
      if (MemDepId < StartId) {
        continue;
      }
      MStream->addAliasInst(MemDepStaticInst);
    }
  }
}

void DynStreamRegionAnalyzer::addIVAccess(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  for (unsigned OperandIdx = 0, NumOperands = StaticInst->getNumOperands();
       OperandIdx != NumOperands; ++OperandIdx) {
    auto OperandValue = StaticInst->getOperand(OperandIdx);
    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {
      auto Iter = this->InstStaticStreamMap.find(PHINode);
      if (Iter != this->InstStaticStreamMap.end()) {
        const auto &DynamicOperand =
            *(DynamicInst->DynamicOperands[OperandIdx]);
        for (auto &SS : Iter->second) {
          auto DynS = this->getDynStreamByStaticStream(SS);
          DynS->addAccess(DynamicOperand);
        }
      } else {
        // This means that this is not an IV stream.
      }
    }
  }
}

void DynStreamRegionAnalyzer::endIter(const llvm::Loop *Loop) {
  assert(this->TopLoop->contains(Loop) &&
         "End iteration for loop outside of TopLoop.");
  auto Iter = this->InnerMostLoopStreamMap.find(Loop);
  if (Iter != this->InnerMostLoopStreamMap.end()) {
    for (auto &SS : Iter->second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      DynS->endIter();
    }
  }
}

void DynStreamRegionAnalyzer::endLoop(const llvm::Loop *Loop) {
  assert(this->TopLoop->contains(Loop) &&
         "End loop for loop outside of TopLoop.");
  auto Iter = this->ConfigureLoopStreamMap.find(Loop);
  if (Iter != this->ConfigureLoopStreamMap.end()) {
    for (auto &SS : Iter->second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      DynS->endStream();
    }
  }
}

void DynStreamRegionAnalyzer::finalizePlan(
    StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy) {
  // Finalize all patterns.
  // This will dump the pattern to file.
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &SS : InstStream.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      DynS->finalizePattern();
    }
  }

  this->markQualifiedStreams(StreamPassQualifySeedStrategy,
                             StreamPassChooseStrategy);
  this->disqualifyStreams();
  if (StreamPassChooseStrategy ==
      StreamPassChooseStrategyE::DYNAMIC_OUTER_MOST) {
    this->chooseStreamAtDynamicOuterMost();
  } else if (StreamPassChooseStrategy ==
             StreamPassChooseStrategyE::STATIC_OUTER_MOST) {
    this->chooseStreamAtStaticOuterMost();
  } else {
    this->chooseStreamAtInnerMost();
  }
  this->buildChosenStreamDependenceGraph();
  this->buildTransformPlan();
  this->buildStreamConfigureLoopInfoMap(this->TopLoop);

  this->dumpTransformPlan();
  this->dumpConfigurePlan();
}

void DynStreamRegionAnalyzer::endTransform() {
  // This will set all the coalesce information.
  this->FuncSE->endAll();
  // Call the base class.
  StaticStreamRegionAnalyzer::endTransform();
}

DynStream *DynStreamRegionAnalyzer::getDynStreamByInstAndConfigureLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfigureLoop) const {
  auto SS = this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop);
  if (SS == nullptr) {
    return nullptr;
  }
  return this->getDynStreamByStaticStream(SS);
}

DynStream *
DynStreamRegionAnalyzer::getDynStreamByStaticStream(StaticStream *SS) const {
  auto Iter = this->StaticDynStreamMap.find(SS);
  assert(Iter != this->StaticDynStreamMap.end() &&
         "Missing DynStream for StaticStream");
  return Iter->second;
}

void DynStreamRegionAnalyzer::markQualifiedStreams(
    StreamPassQualifySeedStrategyE StreamPassQualifySeedStrategy,
    StreamPassChooseStrategyE StreamPassChooseStrategy) {

  LLVM_DEBUG(llvm::dbgs() << "==== SRA: MarkQualifiedStreams\n");

  auto IsQualifySeed = [StreamPassQualifySeedStrategy,
                        StreamPassChooseStrategy](DynStream *S) -> bool {
    if (StreamPassChooseStrategy == StreamPassChooseStrategyE::INNER_MOST) {
      if (S->SStream->ConfigureLoop != S->SStream->InnerMostLoop) {
        // Enforce the inner most loop constraint.
        return false;
      }
    }
    if (StreamPassQualifySeedStrategy ==
        StreamPassQualifySeedStrategyE::STATIC) {
      // Only use static information.
      return S->SStream->isQualified();
    }
    // Use dynamic information.
    return S->isQualifySeed();
  };

  std::list<DynStream *> Queue;
  for (auto &StaticDynStream : this->StaticDynStreamMap) {
    auto DynS = StaticDynStream.second;
    if (IsQualifySeed(DynS)) {
      Queue.emplace_back(DynS);
    }
  }

  /**
   * BFS on the qualified streams.
   */
  while (!Queue.empty()) {
    auto S = Queue.front();
    Queue.pop_front();
    if (S->isQualified()) {
      // We have already processed this stream.
      continue;
    }
    if (!IsQualifySeed(S)) {
      assert(false && "Stream should be a qualify seed to be inserted into the "
                      "qualifying queue.");
      continue;
    }

    LLVM_DEBUG(llvm::dbgs() << "Mark Qualified " << S->getStreamName() << '\n');
    S->markQualified();
    // Check all the dependent streams.
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (IsQualifySeed(DependentStream)) {
        Queue.emplace_back(DependentStream);
      }
    }
  }
}

void DynStreamRegionAnalyzer::disqualifyStreams() {
  /**
   * For IVStreams with base loads, we assume they are qualified from the
   * beginning point. Now it's time to check if their base memory streams are
   * actually qualified. If not, we need to propagate the dequalified signal
   * along the dependence chain.
   */
  std::list<DynStream *> DisqualifiedQueue;
  for (auto &StaticDynStream : this->StaticDynStreamMap) {
    auto DynS = StaticDynStream.second;
    if (DynS->isQualified()) {
      for (auto &BackMemBaseStream : DynS->getBackMemBaseStreams()) {
        // ! So far let only allow directly dependence.
        if (BackMemBaseStream->getBaseStreams().count(DynS) == 0 &&
            DynS->SStream->StaticStreamInfo.val_pattern() !=
                ::LLVM::TDG::StreamValuePattern::REDUCTION) {
          // Purely back-mem dependence streams is disabled now,
          // unless we know it's reduction pattern.
          DisqualifiedQueue.emplace_back(DynS);
          break;
        }
      }
      continue;
    }
    for (auto &BackIVDependentStream : DynS->getBackIVDependentStreams()) {
      if (BackIVDependentStream->isQualified()) {
        // We should disqualify this stream.
        DisqualifiedQueue.emplace_back(BackIVDependentStream);
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
    S->markUnqualified();

    // Propagate to dependent streams.
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (DependentStream->isQualified()) {
        DisqualifiedQueue.emplace_back(DependentStream);
      }
    }

    // Also need to check back edges.
    for (const auto &DependentStream : S->getBackIVDependentStreams()) {
      if (DependentStream->isQualified()) {
        DisqualifiedQueue.emplace_back(DependentStream);
      }
    }
  }
}

void DynStreamRegionAnalyzer::chooseStreamAtInnerMost() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    DynStream *ChosenDynS = nullptr;
    for (auto &SS : InstStream.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      if (DynS->isQualified()) {
        ChosenDynS = DynS;
        break;
      }
    }
    if (ChosenDynS != nullptr) {
      this->InstChosenStreamMap.emplace(Inst, ChosenDynS->SStream);
      ChosenDynS->markChosen();
    }
  }
}

void DynStreamRegionAnalyzer::chooseStreamAtDynamicOuterMost() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    DynStream *ChosenDynS = nullptr;
    for (auto &SS : InstStream.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      if (DynS->isQualified()) {
        // This will make sure we get the outer most qualified stream.
        ChosenDynS = DynS;
      }
    }
    if (ChosenDynS != nullptr) {
      this->InstChosenStreamMap.emplace(Inst, ChosenDynS->SStream);
      ChosenDynS->markChosen();
    }
  }
}

void DynStreamRegionAnalyzer::chooseStreamAtStaticOuterMost() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    DynStream *ChosenDynS = nullptr;
    int ChosenNumQualifiedDepStreams = -1;
    for (auto &SS : InstStream.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      if (DynS->isQualified() && SS->isQualified()) {
        LLVM_DEBUG(llvm::dbgs() << "==== Choose stream StaticOuterMost for "
                                << SS->getStreamName() << '\n');
        /**
         * Instead of just choose the out-most loop, we use
         * a heuristic to select the level with most number
         * of dependent streams qualified.
         * TODO: Fully check all dependent streams.
         */
        int NumQualifiedDepStreams = 0;
        for (auto DepSS : SS->DependentStreams) {
          if (DepSS->isQualified()) {
            NumQualifiedDepStreams++;
            LLVM_DEBUG(llvm::dbgs() << "====== Qualified DepS "
                                    << DepSS->getStreamName() << '\n');
          }
        }
        LLVM_DEBUG(llvm::dbgs()
                   << "======= # Qualified DepS = " << NumQualifiedDepStreams
                   << ", ChosenNumQualifiedDepStreams = "
                   << ChosenNumQualifiedDepStreams << '\n');
        if (NumQualifiedDepStreams >= ChosenNumQualifiedDepStreams) {
          ChosenDynS = DynS;
          ChosenNumQualifiedDepStreams = NumQualifiedDepStreams;
        }
      }
    }
    if (ChosenDynS != nullptr) {
      this->InstChosenStreamMap.emplace(Inst, ChosenDynS->SStream);
      ChosenDynS->markChosen();
      LLVM_DEBUG(llvm::dbgs()
                 << "== Choose " << ChosenDynS->getStreamName() << '\n');
    }
  }
}

void DynStreamRegionAnalyzer::buildChosenStreamDependenceGraph() {
  auto GetChosenStream = [this](const llvm::Instruction *Inst) -> DynStream * {
    auto Iter = this->InstChosenStreamMap.find(Inst);
    if (Iter == this->InstChosenStreamMap.end()) {
      return nullptr;
    }
    auto SS = Iter->second;
    return this->getDynStreamByStaticStream(SS);
  };
  for (auto &InstStaticStream : this->InstChosenStreamMap) {
    auto &SS = InstStaticStream.second;
    auto DynS = this->getDynStreamByStaticStream(SS);
    DynS->buildChosenDependenceGraph(GetChosenStream);
  }
}

FunctionalStreamEngine *DynStreamRegionAnalyzer::getFuncSE() {
  return this->FuncSE.get();
}

std::string
DynStreamRegionAnalyzer::classifyStream(const DynMemStream &S) const {
  if (S.getNumBaseLoads() > 0) {
    // We have dependent base loads here.
    return "INDIRECT";
  }

  // No base loads.
  const auto &BaseInductionVars = S.getBaseInductionVars();
  if (BaseInductionVars.empty()) {
    // No base ivs, should be a constant stream, but here we treat it as AFFINE.
    return "AFFINE";
  }

  if (BaseInductionVars.size() > 1) {
    // Multiple IVs.
    return "MULTI_IV";
  }

  const auto &BaseIV = *BaseInductionVars.begin();
  auto BaseIVStream =
      this->getDynStreamByInstAndConfigureLoop(BaseIV, S.getLoop());
  if (BaseIVStream == nullptr) {
    // This should not happen, but so far make it indirect.
    return "INDIRECT";
  }

  const auto &BaseIVBaseLoads = BaseIVStream->getBaseLoads();
  if (!BaseIVBaseLoads.empty()) {
    for (const auto &BaseIVBaseLoad : BaseIVBaseLoads) {
      if (BaseIVBaseLoad == S.getInst()) {
        return "POINTER_CHASE";
      }
    }
    return "INDIRECT";
  }

  if (BaseIVStream->getPattern().computed()) {
    auto Pattern = BaseIVStream->getPattern().getPattern().ValPattern;
    if (Pattern <= StreamPattern::ValuePattern::QUARDRIC) {
      // The pattern of the IV is AFFINE.
      return "AFFINE";
    }
  }

  return "RANDOM";
}

void DynStreamRegionAnalyzer::finalizeCoalesceInfo() {
  /**
   * In the old trace-based transformation, we use the trace to coalesce
   * streams. However, it lacks coalesce offset, and thus not very efficient
   * and realistic. We will switch to new execution-based coalescing, in which
   * the compiler really analyzes if there is a constant offset.
   */
  // this->getFuncSE()->finalizeCoalesceInfo();

  std::queue<llvm::Loop *> LoopQueue;
  LoopQueue.push(TopLoop);
  while (!LoopQueue.empty()) {
    auto CurrentLoop = LoopQueue.front();
    LoopQueue.pop();
    for (auto &SubLoop : CurrentLoop->getSubLoops()) {
      LoopQueue.push(SubLoop);
    }
    this->coalesceStreamsAtLoop(CurrentLoop);
  }
}

void DynStreamRegionAnalyzer::dumpStats() const {
  std::string StatFilePath = this->getAnalyzePath() + "/stats.txt";

  std::ofstream O(StatFilePath);
  assert(O.is_open() && "Failed to open stats file.");

  O << "--------------- Stats -----------------\n";
  /**
   * Loop
   * Inst
   * AddrPat
   * AccPat
   * Iters
   * Accs
   * Updates
   * StreamCount
   * LoopLevel
   * BaseLoads
   * StreamClass
   * Footprint
   * AddrInsts
   * AliasInsts
   * Qualified
   * Chosen
   * PossiblePaths
   */

  O << "--------------- Stream -----------------\n";
  for (auto &InstStaticStreamEntry : this->InstStaticStreamMap) {
    if (llvm::isa<llvm::PHINode>(InstStaticStreamEntry.first)) {
      continue;
    }
    for (auto &SS : InstStaticStreamEntry.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      auto &Stream = *reinterpret_cast<DynMemStream *>(DynS);
      O << LoopUtils::getLoopId(Stream.getLoop());
      O << ' ' << Utils::formatLLVMInst(Stream.getInst());

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = Stream.getTotalIters();
      size_t Accesses = Stream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = Stream.getTotalStreams();
      size_t LoopLevel = 0;
      size_t BaseLoads = Stream.getNumBaseLoads();

      std::string StreamClass = this->classifyStream(Stream);
      size_t Footprint = Stream.getFootprint().getNumCacheLinesAccessed();
      size_t AddrInsts = Stream.getNumAddrInsts();

      std::string Qualified = "NO";
      if (Stream.getQualified()) {
        Qualified = "YES";
      }

      std::string Chosen = Stream.isChosen() ? "YES" : "NO";

      int LoopPossiblePaths = -1;

      size_t AliasInsts = Stream.getAliasInsts().size();

      if (Stream.getPattern().computed()) {
        const auto &Pattern = Stream.getPattern().getPattern();
        AddrPattern = StreamPattern::formatValuePattern(Pattern.ValPattern);
        AccPattern = StreamPattern::formatAccessPattern(Pattern.AccPattern);
        Updates = Pattern.Updates;
      }
      O << ' ' << AddrPattern;
      O << ' ' << AccPattern;
      O << ' ' << Iters;
      O << ' ' << Accesses;
      O << ' ' << Updates;
      O << ' ' << StreamCount;
      O << ' ' << LoopLevel;
      O << ' ' << BaseLoads;
      O << ' ' << StreamClass;
      O << ' ' << Footprint;
      O << ' ' << AddrInsts;
      O << ' ' << AliasInsts;
      O << ' ' << Qualified;
      O << ' ' << Chosen;
      O << ' ' << LoopPossiblePaths;
      O << '\n';
    }
  }

  O << "--------------- Loop -----------------\n";
  O << "--------------- IV Stream ------------\n";
  for (auto &InstStaticStreamEntry : this->InstStaticStreamMap) {
    if (!llvm::isa<llvm::PHINode>(InstStaticStreamEntry.first)) {
      continue;
    }
    for (auto &SS : InstStaticStreamEntry.second) {
      auto DynS = this->getDynStreamByStaticStream(SS);
      auto &IVStream = *reinterpret_cast<DynIndVarStream *>(DynS);
      O << LoopUtils::getLoopId(IVStream.getLoop());
      O << ' ' << Utils::formatLLVMInst(IVStream.getPHIInst());

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = IVStream.getTotalIters();
      size_t Accesses = IVStream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = IVStream.getTotalStreams();

      size_t ComputeInsts = IVStream.getComputeInsts().size();

      std::string StreamClass = "whatever";
      if (IVStream.getPattern().computed()) {
        const auto &Pattern = IVStream.getPattern().getPattern();
        AddrPattern = StreamPattern::formatValuePattern(Pattern.ValPattern);
        AccPattern = StreamPattern::formatAccessPattern(Pattern.AccPattern);
        Updates = Pattern.Updates;
      }

      int LoopPossiblePaths = -1;

      O << ' ' << AddrPattern;
      O << ' ' << AccPattern;
      O << ' ' << Iters;
      O << ' ' << Accesses;
      O << ' ' << Updates;
      O << ' ' << StreamCount;
      O << ' ' << StreamClass;
      O << ' ' << ComputeInsts;
      O << ' ' << LoopPossiblePaths;
      O << '\n';
    }
  }
  O.close();
}

#undef DEBUG_TYPE