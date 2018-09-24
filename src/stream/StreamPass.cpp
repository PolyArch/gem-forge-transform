#include "stream/StreamPass.h"

#include "llvm/Support/FileSystem.h"

#include <iomanip>
#include <sstream>

#define DEBUG_TYPE "StreamPass"

std::string StreamTransformPlan::format() const {
  std::stringstream ss;
  ss << std::setw(10) << std::left
     << StreamTransformPlan::formatPlanT(this->Plan);
  for (auto UsedStream : this->UsedStreams) {
    ss << UsedStream->formatName() << ' ';
  }
  return ss.str();
}

bool StreamPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);
  this->MemorizedStreamInst.clear();
  return Ret;
}

bool StreamPass::finalize(llvm::Module &Module) {
  return ReplayTrace::finalize(Module);
}

std::string StreamPass::classifyStream(const MemStream &S) const {
  const auto &Loop = S.getLoop();

  if (S.getNumBaseLoads() > 1) {
    return "MULTI_BASE";
  } else if (S.getNumBaseLoads() == 1) {
    const auto &BaseLoad = *S.getBaseLoads().begin();
    if (BaseLoad == S.getInst()) {
      // Chasing myself.
      return "POINTER_CHASE";
    }
    auto BaseStreamsIter = this->InstMemStreamMap.find(BaseLoad);
    if (BaseStreamsIter != this->InstMemStreamMap.end()) {
      const auto &BaseStreams = BaseStreamsIter->second;
      for (const auto &BaseStream : BaseStreams) {
        if (BaseStream.getLoop() == S.getLoop()) {
          if (BaseStream.getNumBaseLoads() != 0) {
            return "CHAIN_BASE";
          }
          if (BaseStream.getPattern().computed()) {
            auto Pattern = BaseStream.getPattern().getPattern().ValPattern;
            if (Pattern <= StreamPattern::ValuePattern::QUARDRIC) {
              return "AFFINE_BASE";
            }
          }
        }
      }
    }
    return "RANDOM_BASE";
  } else {
    auto BaseInductionVars = S.getBaseInductionVars();
    if (BaseInductionVars.size() > 1) {
      return "MULTI_IV";
    } else if (BaseInductionVars.size() == 1) {
      const auto &BaseIV = *BaseInductionVars.begin();
      auto BaseIVStreamIter = this->PHINodeIVStreamMap.find(BaseIV);
      if (BaseIVStreamIter != this->PHINodeIVStreamMap.end()) {
        for (const auto &BaseIVStream : BaseIVStreamIter->second) {
          if (BaseIVStream.getLoop() != S.getLoop()) {
            continue;
          }
          if (BaseIVStream.getPattern().computed()) {
            auto Pattern = BaseIVStream.getPattern().getPattern().ValPattern;
            if (Pattern <= StreamPattern::ValuePattern::QUARDRIC) {
              return "AFFINE_IV";
            }
          }
        }
      }
      return "RANDOM_IV";
    } else {
      if (S.getPattern().computed()) {
        auto Pattern = S.getPattern().getPattern().ValPattern;
        if (Pattern <= StreamPattern::ValuePattern::QUARDRIC) {
          return "AFFINE";
        } else {
          return "RANDOM";
        }
      } else {
        return "RANDOM";
      }
    }
  }
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
   */

  O << "--------------- Stream -----------------\n";
  for (auto &InstStreamEntry : this->InstMemStreamMap) {
    for (auto &Stream : InstStreamEntry.second) {
      O << LoopUtils::getLoopId(Stream.getLoop());
      O << ' ' << LoopUtils::formatLLVMInst(Stream.getInst());

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = Stream.getTotalIters();
      size_t Accesses = Stream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = Stream.getTotalStreams();
      size_t LoopLevel = Stream.getLoopLevel();
      size_t BaseLoads = Stream.getNumBaseLoads();

      std::string StreamClass = this->classifyStream(Stream);
      size_t Footprint = Stream.getFootprint().getNumCacheLinesAccessed();
      size_t AddrInsts = Stream.getNumAddrInsts();

      std::string Qualified = "NO";
      if (Stream.getQualified()) {
        Qualified = "YES";
      }

      std::string Chosen = "NO";
      {
        auto ChosenLoopInstStreamIter =
            this->ChosenLoopInstStream.find(Stream.getLoop());
        if (ChosenLoopInstStreamIter != this->ChosenLoopInstStream.end()) {
          if (ChosenLoopInstStreamIter->second.count(Stream.getInst()) != 0) {
            Chosen = "YES";
          }
        }
      }

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
      O << '\n';
    }
  }

  // Also dump statistics for all the unanalyzed memory accesses.
  size_t TotalAccesses = 0;
  for (const auto &Entry : this->MemAccessInstCount) {
    const auto &Inst = Entry.first;
    TotalAccesses += Entry.second;
    if (this->InstMemStreamMap.count(Inst) != 0) {
      // This instruction is already dumped.
      if (!this->InstMemStreamMap.at(Inst).empty()) {
        continue;
      }
    }
    std::string LoopId = "UNKNOWN";
    std::string StreamClass = "NOT_STREAM";
    auto LI = this->CachedLI->getLoopInfo(
        const_cast<llvm::Function *>(Inst->getFunction()));
    auto Loop = LI->getLoopFor(Inst->getParent());
    if (Loop != nullptr) {
      LoopId = LoopUtils::getLoopId(Loop);
      StreamClass = "INCONTINUOUS";
    }
    O << LoopId;
    O << ' ' << LoopUtils::formatLLVMInst(Inst); // Inst
    O << ' ' << "NOT_STREAM";                    // Address pattern
    O << ' ' << "NOT_STREAM";                    // Access pattern
    O << ' ' << 0;                               // Iters
    O << ' ' << Entry.second;                    // Accesses
    O << ' ' << 0;                               // Updates
    O << ' ' << 0;                               // StreamCount
    O << ' ' << -1;                              // LoopLevel
    O << ' ' << -1;                              // BaseLoads
    O << ' ' << StreamClass;                     // Stream class
    O << ' ' << 0;                               // Footprint
    O << ' ' << -1;                              // AddrInsts
    O << ' ' << -1;                              // AliasInsts
    O << ' ' << "NO";                            // Qualified
    O << ' ' << "NO" << '\n';                    // Chosen
  }

  assert(TotalAccesses == this->DynMemInstCount && "Mismatch total accesses.");

  O << "--------------- Loop -----------------\n";
  O << "--------------- IV Stream ------------\n";
  for (auto &PHINodeIVStreamListEntry : this->PHINodeIVStreamMap) {
    auto &PHINode = PHINodeIVStreamListEntry.first;
    for (auto &IVStream : PHINodeIVStreamListEntry.second) {
      O << LoopUtils::getLoopId(IVStream.getLoop());
      O << ' ' << LoopUtils::formatLLVMInst(IVStream.getPHIInst());

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
      O << ' ' << AddrPattern;
      O << ' ' << AccPattern;
      O << ' ' << Iters;
      O << ' ' << Accesses;
      O << ' ' << Updates;
      O << ' ' << StreamCount;
      O << ' ' << StreamClass;
      O << ' ' << ComputeInsts;
      O << '\n';
    }
  }
}

void StreamPass::transform() {
  this->analyzeStream();
  this->chooseStream();
  this->buildChosenStreamDependenceGraph();
  this->buildAllChosenStreamDependenceGraph();
  /**
   * This has to be finalized after we build the chosen stream dependence
   * graphs, which will be dumped into the info file of the stream.
   */
  for (auto &InstStreamEntry : this->InstStreamMap) {
    for (auto &S : InstStreamEntry.second) {
      S->finalize(this->Trace->DataLayout);
    }
  }
  this->buildAddressInterpreterForChosenStreams();

  this->makeStreamTransformPlan();

  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
    std::unordered_set<Stream *> SortedStreams;
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      auto S = InstStreamEntry.second;
      if (SortedStreams.count(S) != 0) {
        continue;
      }
      std::list<Stream *> Stack;
      this->sortChosenStream(S, Stack, SortedStreams);
    }
    DEBUG(this->DEBUG_SORTED_STREAMS_FOR_LOOP(LoopInstStreamEntry.first));
  }

  delete this->Trace;
  this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  this->transformStream();
}

void StreamPass::initializeMemStreamIfNecessary(const LoopStackT &LoopStack,
                                                llvm::Instruction *Inst) {
  assert(Utils::isMemAccessInst(Inst) &&
         "Try to initialize memory stream for non-memory access instruction.");
  {
    // Initialize the global count.
    auto Iter = this->MemAccessInstCount.find(Inst);
    if (Iter == this->MemAccessInstCount.end()) {
      this->MemAccessInstCount.emplace(Inst, 0);
    }
  }

  auto Iter = this->InstMemStreamMap.find(Inst);
  if (Iter == this->InstMemStreamMap.end()) {
    Iter = this->InstMemStreamMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                        std::forward_as_tuple())
               .first;
    this->InstStreamMap.emplace(std::piecewise_construct,
                                std::forward_as_tuple(Inst),
                                std::forward_as_tuple());
  }

  auto &Streams = Iter->second;
  auto LoopIter = LoopStack.rbegin();
  for (const auto &Stream : Streams) {
    if (LoopIter == LoopStack.rend()) {
      break;
    }
    assert(Stream.getLoop() == (*LoopIter) && "Mismatch initialized stream.");
    ++LoopIter;
  }

  // Initialize the remaining loops.
  auto IsInductionVar = [this](const llvm::PHINode *PHINode) -> bool {
    return this->PHINodeIVStreamMap.count(PHINode) != 0;
    // return false;
  };
  while (LoopIter != LoopStack.rend()) {
    auto LoopLevel = Streams.size();
    const llvm::Loop *InnerMostLoop = *LoopIter;
    if (LoopLevel > 0) {
      InnerMostLoop = Streams.front().getInnerMostLoop();
    }
    Streams.emplace_back(this->OutputExtraFolderPath, Inst, *LoopIter,
                         InnerMostLoop, LoopLevel, IsInductionVar);
    this->InstStreamMap.at(Inst).emplace_back(&Streams.back());
    ++LoopIter;
  }
}
void StreamPass::initializeIVStreamIfNecessary(const LoopStackT &LoopStack,
                                               llvm::PHINode *Inst) {

  auto Iter = this->PHINodeIVStreamMap.find(Inst);
  if (Iter == this->PHINodeIVStreamMap.end()) {
    Iter = this->PHINodeIVStreamMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                        std::forward_as_tuple())
               .first;
    this->InstStreamMap.emplace(std::piecewise_construct,
                                std::forward_as_tuple(Inst),
                                std::forward_as_tuple());
  }

  auto &Streams = Iter->second;
  auto LoopIter = LoopStack.rbegin();
  for (const auto &Stream : Streams) {
    if (LoopIter == LoopStack.rend()) {
      break;
    }
    assert(Stream.getLoop() == (*LoopIter) && "Mismatch initialized stream.");
    ++LoopIter;
  }

  // Initialize the remaining loops.
  auto IsInductionVar = [this](llvm::PHINode *PHINode) -> bool {
    // return this->PHINodeIVStreamMap.count(PHINode) != 0;
    return false;
  };
  while (LoopIter != LoopStack.rend()) {
    auto LoopLevel = Streams.size();
    auto Loop = *LoopIter;
    auto Level = Streams.size();
    const llvm::Loop *InnerMostLoop = *LoopIter;
    if (LoopLevel > 0) {
      InnerMostLoop = Streams.front().getInnerMostLoop();
    }
    auto ComputeInsts = InductionVarStream::searchComputeInsts(Inst, Loop);
    if (InductionVarStream::isInductionVarStream(Inst, ComputeInsts)) {
      Streams.emplace_back(this->OutputExtraFolderPath, Inst, Loop,
                           InnerMostLoop, Level, std::move(ComputeInsts));
      this->InstStreamMap.at(Inst).emplace_back(&Streams.back());
      DEBUG(llvm::errs() << "Initialize IVStream "
                         << Streams.back().formatName() << '\n');
    }
    ++LoopIter;
  }
  DEBUG(llvm::errs() << "Initialize IVStream returned\n");
}

void StreamPass::activateStream(ActiveStreamMapT &ActiveStreams,
                                llvm::Instruction *Inst) {

  auto InstMemStreamMapIter = this->InstMemStreamMap.find(Inst);
  assert(InstMemStreamMapIter != this->InstMemStreamMap.end() &&
         "Failed to look up streams to activate.");

  for (auto &S : InstMemStreamMapIter->second) {
    const auto &Loop = S.getLoop();
    auto ActiveStreamsIter = ActiveStreams.find(Loop);
    if (ActiveStreamsIter == ActiveStreams.end()) {
      ActiveStreamsIter =
          ActiveStreams
              .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                       std::forward_as_tuple())
              .first;
    }
    auto &ActiveStreamsInstMap = ActiveStreamsIter->second;

    auto ActiveStreamsInstMapIter = ActiveStreamsInstMap.find(Inst);
    if (ActiveStreamsInstMap.count(Inst) == 0) {
      ActiveStreamsInstMap.emplace(Inst, &S);
    } else {
      // Otherwise, the stream is already activated.
      // Ignore this case.
    }
  }
}

void StreamPass::activateIVStream(ActiveIVStreamMapT &ActiveIVStreams,
                                  llvm::PHINode *PHINode) {
  auto PHINodeIVStreamMapIter = this->PHINodeIVStreamMap.find(PHINode);
  assert(PHINodeIVStreamMapIter != this->PHINodeIVStreamMap.end() &&
         "Failed to look up IV streams to activate.");

  auto DynamicVal =
      this->Trace->DynamicFrameStack.front().getValueNullable(PHINode);
  uint64_t Value;
  if (DynamicVal != nullptr) {
    Value = std::stoul(DynamicVal->Value);
  }

  for (auto &S : PHINodeIVStreamMapIter->second) {
    auto *IVStream = &S;
    const auto &Loop = IVStream->getLoop();
    auto ActiveIVStreamsIter = ActiveIVStreams.find(Loop);
    if (ActiveIVStreamsIter == ActiveIVStreams.end()) {
      ActiveIVStreamsIter =
          ActiveIVStreams
              .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                       std::forward_as_tuple())
              .first;
    }
    auto &ActivePHINodeIVStreamMap = ActiveIVStreamsIter->second;
    if (ActivePHINodeIVStreamMap.count(PHINode) == 0) {
      // Activate the stream.
      // DEBUG(llvm::errs() << "Activate iv stream " << IVStream->formatName()
      //                    << "\n");
      ActivePHINodeIVStreamMap.emplace(PHINode, IVStream);
      DEBUG(llvm::errs() << "Activate IVStream " << IVStream->formatName()
                         << '\n');
    }
    // if (DynamicVal != nullptr) {
    //   IVStream->addAccess(Value);
    // }
  }
}

void StreamPass::pushLoopStack(LoopStackT &LoopStack,
                               ActiveStreamMapT &ActiveStreams,
                               ActiveIVStreamMapT &ActiveIVStreams,
                               llvm::Loop *Loop) {

  LoopStack.emplace_back(Loop);

  if (this->InitializedLoops.count(Loop) == 0) {
    // Find the memory access belongs to this level.
    auto &StreamInsts =
        this->MemorizedStreamInst
            .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                     std::forward_as_tuple())
            .first->second;
    const auto &SubLoops = Loop->getSubLoops();
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      bool IsAtThisLevel = true;
      for (const auto &SubLoop : SubLoops) {
        if (SubLoop->contains(BB)) {
          IsAtThisLevel = false;
          break;
        }
      }
      if (!IsAtThisLevel) {
        continue;
      }
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        auto StaticInst = &*InstIter;
        if (!Utils::isMemAccessInst(StaticInst)) {
          continue;
        }
        StreamInsts.insert(StaticInst);
      }
    }

    auto HeaderBB = Loop->getHeader();
    auto PHINodes = HeaderBB->phis();
    for (auto PHINodeIter = PHINodes.begin(), PHINodeEnd = PHINodes.end();
         PHINodeIter != PHINodeEnd; ++PHINodeIter) {
      auto PHINode = &*PHINodeIter;
      auto ComputeInsts = InductionVarStream::searchComputeInsts(PHINode, Loop);
      if (InductionVarStream::isInductionVarStream(PHINode, ComputeInsts)) {
        StreamInsts.insert(PHINode);
      }
    }

    this->InitializedLoops.insert(Loop);
  }
  auto Iter = this->MemorizedStreamInst.find(Loop);
  assert(Iter != this->MemorizedStreamInst.end() &&
         "An initialized loop should have memorized memory accesses.");
  /**
   * Since the mem stream may be dependent on IVStream, we have to
   * initialize all the IVStreams first.
   */
  for (auto StaticInst : Iter->second) {
    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(StaticInst)) {
      this->initializeIVStreamIfNecessary(LoopStack, PHINode);
      this->activateIVStream(ActiveIVStreams, PHINode);
    }
  }

  /**
   * Initialize and activate the normal streams.
   */

  for (auto StaticInst : Iter->second) {
    if (!llvm::isa<llvm::PHINode>(StaticInst)) {
      assert(
          Utils::isMemAccessInst(StaticInst) &&
          "Should be memory access instruction if not an induction variable.");
      this->initializeMemStreamIfNecessary(LoopStack, StaticInst);
      this->activateStream(ActiveStreams, StaticInst);
    }
  }
}

void StreamPass::addAccess(const LoopStackT &LoopStack,
                           ActiveStreamMapT &ActiveStreams,
                           DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  this->initializeMemStreamIfNecessary(LoopStack, StaticInst);
  this->MemAccessInstCount.at(StaticInst)++;
  this->activateStream(ActiveStreams, StaticInst);
  auto Iter = this->InstMemStreamMap.find(StaticInst);
  if (Iter == this->InstMemStreamMap.end()) {
    // This is possible as some times we have instructions not in a loop.
    return;
  }
  uint64_t Addr = Utils::getMemAddr(DynamicInst);
  for (auto &Stream : Iter->second) {
    Stream.addAccess(Addr, DynamicInst->getId());
  }
}

void StreamPass::handleAlias(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  auto Iter = this->InstMemStreamMap.find(StaticInst);
  if (Iter == this->InstMemStreamMap.end()) {
    // This is possible as some times we have instructions not in a loop.
    return;
  }
  for (auto &S : Iter->second) {
    const auto &Loop = S.getLoop();
    const auto StartId = S.getStartId();
    if (StartId == DynamicInstruction::InvalidId) {
      // This stream is not even active (or more precisely, has not seen the
      // first access).
      continue;
    }
    // Iterate the memory dependence.
    auto MemDepsIter = this->Trace->MemDeps.find(DynamicInst->getId());
    if (MemDepsIter == this->Trace->MemDeps.end()) {
      // There is no known alias.
      break;
    }
    for (const auto &MemDepId : MemDepsIter->second) {
      // Check if this dynamic instruction is still alive.
      auto MemDepDynamicInst = this->Trace->getAliveDynamicInst(MemDepId);
      if (MemDepDynamicInst == nullptr) {
        // This dynamic instruction is already committed. We assume that the
        // dependence is old enough to be safely ignored.
        continue;
      }
      auto MemDepStaticInst = MemDepDynamicInst->getStaticInstruction();
      assert(MemDepStaticInst != nullptr &&
             "Invalid memory dependent llvm static instruction.");
      // Ignore those instruction not from our loop.
      if (!Loop->contains(MemDepStaticInst)) {
        continue;
      }
      // They are from the same level of loop.
      // Check if they are older than the start id.
      if (MemDepId < StartId) {
        continue;
      }

      S.addAliasInst(MemDepStaticInst);
    }
  }
}

void StreamPass::endIter(const LoopStackT &LoopStack,
                         ActiveStreamMapT &ActiveStreams,
                         ActiveIVStreamMapT &ActiveIVStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when endIter is called.");
  const auto &EndedLoop = LoopStack.back();
  {
    auto ActiveStreamsIter = ActiveStreams.find(EndedLoop);
    if (ActiveStreamsIter != ActiveStreams.end()) {
      for (auto &InstStreamEntry : ActiveStreamsIter->second) {
        auto &Stream = InstStreamEntry.second;
        if (Stream->getLoopLevel() != 0) {
          continue;
        }
        auto Inst = Stream->getInst();
        for (auto &S :
             this->InstMemStreamMap.at(const_cast<llvm::Instruction *>(Inst))) {
          S.endIter();
          if (S.getLoop() == LoopStack.front()) {
            break;
          }
        }
      }
    }
  }
  {
    auto ActiveIVStreamsIter = ActiveIVStreams.find(EndedLoop);
    if (ActiveIVStreamsIter != ActiveIVStreams.end()) {
      for (auto &PHINodeIVStreamEntry : ActiveIVStreamsIter->second) {
        auto &IVStream = PHINodeIVStreamEntry.second;
        if (IVStream->getLoopLevel() != 0) {
          continue;
        }
        auto Inst = IVStream->getPHIInst();
        for (auto &S : this->PHINodeIVStreamMap.at(Inst)) {
          // DEBUG(llvm::errs()
          //       << "End iteration for IVStream " << S.formatName() << '\n');
          S.endIter();
          if (S.getLoop() == LoopStack.front()) {
            break;
          }
        }
      }
    }
  }
}

void StreamPass::popLoopStack(LoopStackT &LoopStack,
                              ActiveStreamMapT &ActiveStreams,
                              ActiveIVStreamMapT &ActiveIVStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when popLoopStack is called.");
  this->endIter(LoopStack, ActiveStreams, ActiveIVStreams);

  // Deactivate all the streams in this loop level.
  const auto &EndedLoop = LoopStack.back();
  auto ActiveStreamsIter = ActiveStreams.find(EndedLoop);
  if (ActiveStreamsIter != ActiveStreams.end()) {
    for (auto &InstStreamEntry : ActiveStreamsIter->second) {
      InstStreamEntry.second->endStream();
    }
    ActiveStreams.erase(ActiveStreamsIter);
  }

  // Deactivate all the iv streams in this loop level.
  auto ActiveIVStreamsIter = ActiveIVStreams.find(EndedLoop);
  if (ActiveIVStreamsIter != ActiveIVStreams.end()) {
    for (auto &PHINodeIVStreamEntry : ActiveIVStreamsIter->second) {
      PHINodeIVStreamEntry.second->endStream();
    }
    ActiveIVStreams.erase(ActiveIVStreamsIter);
  }
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
  ActiveStreamMapT ActiveStreams;
  ActiveIVStreamMapT ActiveIVStreams;

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
        this->popLoopStack(LoopStack, ActiveStreams, ActiveIVStreams);
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Trace->commitOneDynamicInst();
      }
      break;
    }

    while (!LoopStack.empty()) {
      if (LoopStack.back()->contains(NewStaticInst)) {
        break;
      }
      this->popLoopStack(LoopStack, ActiveStreams, ActiveIVStreams);
    }

    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back() != NewLoop) {
        // A new loop.
        this->pushLoopStack(LoopStack, ActiveStreams, ActiveIVStreams, NewLoop);
      } else {
        // This means that we are at a new iteration.
        this->endIter(LoopStack, ActiveStreams, ActiveIVStreams);
      }
    }

    if (IsMemAccess) {
      this->DynMemInstCount++;
      this->addAccess(LoopStack, ActiveStreams, NewDynamicInst);
      this->handleAlias(NewDynamicInst);
      this->CacheWarmerPtr->addAccess(Utils::getMemAddr(NewDynamicInst));
    }

    /**
     * Handle the value for induction variable streams.
     */
    if (!LoopStack.empty()) {
      for (unsigned OperandIdx = 0,
                    NumOperands = NewStaticInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto OperandValue = NewStaticInst->getOperand(OperandIdx);
        if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {
          auto PHINodeIVStreamMapIter = this->PHINodeIVStreamMap.find(PHINode);
          if (PHINodeIVStreamMapIter != this->PHINodeIVStreamMap.end()) {
            uint64_t Value =
                std::stoul(NewDynamicInst->DynamicOperands[OperandIdx]->Value);
            for (auto &IVStream : PHINodeIVStreamMapIter->second) {
              // DEBUG(llvm::errs() << "Add iv access " << IVStream.formatName()
              //                    << " with value " << Value << '\n');
              IVStream.addAccess(Value);
            }
          }
        }
      }
    }
  }
}

MemStream *StreamPass::getMemStreamByInstLoop(llvm::Instruction *Inst,
                                              const llvm::Loop *Loop) {
  auto Iter = this->InstMemStreamMap.find(Inst);
  if (Iter == this->InstMemStreamMap.end()) {
    return nullptr;
  }
  auto &Streams = Iter->second;
  for (auto &S : Streams) {
    if (S.getLoop() == Loop) {
      return &S;
    }
  }
  llvm_unreachable("Failed to find the stream at specified loop level.");
}

void StreamPass::buildStreamDependenceGraph() {
  for (auto &InstStreamEntry : this->InstMemStreamMap) {
    auto &Streams = InstStreamEntry.second;
    for (auto &S : Streams) {
      auto Loop = S.getLoop();
      /**
       * First handle the base IV stream.
       */
      for (const auto &BasePHINode : S.getBaseInductionVars()) {
        auto PHINodeIVStreamMapIter =
            this->PHINodeIVStreamMap.find(BasePHINode);
        if (PHINodeIVStreamMapIter != this->PHINodeIVStreamMap.end()) {
          for (auto &IVStream : PHINodeIVStreamMapIter->second) {
            if (IVStream.getLoop() != Loop) {
              continue;
            }
            DEBUG(llvm::errs() << "Add IV dependence " << S.formatName()
                               << " -> " << IVStream.formatName() << "\n");
            S.addBaseStream(&IVStream);
          }
        }
      }

      /**
       * Handle the base memory stream.
       */
      for (const auto &BaseLoad : S.getBaseLoads()) {
        auto BaseStream = this->getMemStreamByInstLoop(BaseLoad, Loop);
        if (BaseStream != nullptr) {
          DEBUG(llvm::errs() << "Add MEM dependence " << S.formatName()
                             << " -> " << BaseStream->formatName() << "\n");
          S.addBaseStream(BaseStream);
        }
      }
    }
  }
}

void StreamPass::markQualifiedStream() {
  std::list<Stream *> Queue;
  /**
   * First check all the IV streams.
   */
  for (auto &PHINodeIVStreamListEntry : this->PHINodeIVStreamMap) {
    for (auto &IVStream : PHINodeIVStreamListEntry.second) {
      const auto &Pattern = IVStream.getPattern();
      if (!Pattern.computed()) {
        // Only consider the computed pattern.
        continue;
      }
      auto AddrPattern = Pattern.getPattern().ValPattern;
      if (AddrPattern > StreamPattern::ValuePattern::UNKNOWN &&
          AddrPattern <= StreamPattern::ValuePattern::QUARDRIC) {
        // This is affine induction variable.
        Queue.emplace_back(&IVStream);
      }
    }
  }
  /**
   * Also use direct affine stream as the start point to propagate qualified
   * flag in the dependence graph.
   */
  for (auto &InstStreamEntry : this->InstMemStreamMap) {
    auto &Streams = InstStreamEntry.second;
    for (auto &S : Streams) {
      if (S.isAliased()) {
        continue;
      }
      // Also check if it dependends on other streams.
      if (!S.getBaseStreams().empty()) {
        continue;
      }
      // Direct stream without alias or base streams.
      const auto &MemPattern = S.getPattern();
      if (!MemPattern.computed()) {
        continue;
      }
      auto AddrPattern = MemPattern.getPattern().ValPattern;
      if (AddrPattern <= StreamPattern::ValuePattern::QUARDRIC) {
        // This is affine. Push into the queue as start point.
        Queue.emplace_back(&S);
      }
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
    DEBUG(llvm::errs() << "Mark stream " << S->formatName() << " qualified.\n");
    S->markQualified();
    // Check all the dependent streams
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (S->isAliased()) {
        continue;
      }
      bool Qualified = true;
      for (const auto &BaseStream : DependentStream->getBaseStreams()) {
        if (!BaseStream->isQualified()) {
          Qualified = false;
          break;
        }
      }
      if (Qualified) {
        Queue.emplace_back(DependentStream);
      }
    }
  }
}

void StreamPass::addChosenStream(const llvm::Loop *Loop,
                                 const llvm::Instruction *Inst, Stream *S) {
  auto ChosenLoopInstStreamIter = this->ChosenLoopInstStream.find(Loop);
  if (ChosenLoopInstStreamIter == this->ChosenLoopInstStream.end()) {
    ChosenLoopInstStreamIter =
        this->ChosenLoopInstStream
            .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                     std::forward_as_tuple())
            .first;
  }
  assert(ChosenLoopInstStreamIter->second.count(Inst) == 0 &&
         "This stream is already chosen.");
  assert(this->ChosenInstStreamMap.count(Inst) == 0 &&
         "The chosen stream is already inserted into ChosenInstStreamMap.");
  this->ChosenInstStreamMap.emplace(Inst, S);
  DEBUG(llvm::errs() << "Add chosen stream " << S->formatName() << '\n');
  ChosenLoopInstStreamIter->second.emplace(Inst, S);
  S->markChosen();
}

void StreamPass::chooseStream() {
  this->buildStreamDependenceGraph();
  this->markQualifiedStream();

  // First pick all the IVStreams.
  for (auto &PHINodeIVStreamListEntry : this->PHINodeIVStreamMap) {
    auto &IVStreams = PHINodeIVStreamListEntry.second;
    InductionVarStream *ChosenStream = nullptr;
    for (auto &S : IVStreams) {
      if (S.isQualified()) {
        ChosenStream = &S;
      }
    }
    if (ChosenStream != nullptr) {
      this->addChosenStream(ChosenStream->getLoop(), ChosenStream->getPHIInst(),
                            ChosenStream);
    }
  }

  for (auto &InstStreamEntry : this->InstMemStreamMap) {
    auto &Streams = InstStreamEntry.second;
    // Try to pick the highest level of qualified stream.
    MemStream *ChosenStream = nullptr;
    for (auto &S : Streams) {
      if (S.getQualified()) {
        ChosenStream = &S;
      }
    }
    if (ChosenStream != nullptr) {
      auto Loop = ChosenStream->getLoop();
      auto Inst = ChosenStream->getInst();
      this->addChosenStream(Loop, Inst, ChosenStream);
    }
  }
}
void StreamPass::buildChosenStreamDependenceGraph() {
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
    auto Loop = LoopInstStreamEntry.first;
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      auto SelfInst = InstStreamEntry.first;
      auto S = InstStreamEntry.second;
      assert(S->isChosen() &&
             "Streams in ChosenLoopInstStream should be chosen.");
      for (const auto &BaseStream : S->getBaseStreams()) {
        auto BaseInst = BaseStream->getInst();
        if (BaseInst == SelfInst) {
          // Not including myself.
          continue;
        }
        assert(this->InstStreamMap.count(BaseInst) != 0 &&
               "Missing base stream in InstStreamMap.");
        bool FoundChosenBase = false;
        for (auto &BS : this->InstStreamMap.at(BaseInst)) {
          if (BS->isChosen()) {
            assert(BS->getLoop()->contains(S->getLoop()) &&
                   "Chosen base stream should have a higher loop level than "
                   "the original stream.");
            FoundChosenBase = true;
            S->addChosenBaseStream(BS);
            break;
          }
        }
        assert(FoundChosenBase && "Failed to find a chosen base stream.");
      }
    }
  }
}

void StreamPass::buildAllChosenStreamDependenceGraph() {
  std::unordered_set<Stream *> HandledStreams;
  std::list<std::pair<Stream *, bool>> DFSStack;
  std::unordered_set<Stream *> InStackStreams;
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      auto S = InstStreamEntry.second;
      if (HandledStreams.count(S) != 0) {
        continue;
      }
      DFSStack.clear();
      DFSStack.emplace_back(S, false);
      InStackStreams.clear();
      InStackStreams.insert(S);
      while (!DFSStack.empty()) {
        auto &Entry = DFSStack.back();
        if (Entry.second == false) {
          // First time.
          Entry.second = true;
          for (auto &ChosenBaseStream : Entry.first->getChosenBaseStreams()) {
            // if (InStackStreams.count(ChosenBaseStream) != 0) {
            // }
            assert(InStackStreams.count(ChosenBaseStream) == 0 &&
                   "Recursion found in chosen streams.");
            if (HandledStreams.count(ChosenBaseStream) == 0) {
              DFSStack.emplace_back(ChosenBaseStream, 0);
              InStackStreams.insert(ChosenBaseStream);
            }
          }
        } else {
          // Second time.
          for (auto &ChosenBaseStream : Entry.first->getChosenBaseStreams()) {
            Entry.first->addAllChosenBaseStream(ChosenBaseStream);
            for (auto &SS : ChosenBaseStream->getAllChosenBaseStreams()) {
              Entry.first->addAllChosenBaseStream(SS);
            }
          }
          InStackStreams.erase(Entry.first);
          HandledStreams.insert(Entry.first);
          DFSStack.pop_back();
        }
      }
    }
  }
}

std::string StreamPass::getAddressModuleName() const {
  return this->OutTraceName + ".address.bc";
}

void StreamPass::buildAddressInterpreterForChosenStreams() {
  auto &Context = this->Module->getContext();
  auto AddressModule =
      std::make_unique<llvm::Module>(this->getAddressModuleName(), Context);

  for (const auto &InstMemStreamEntry : this->InstMemStreamMap) {
    for (const auto &S : InstMemStreamEntry.second) {
      if (S.isChosen()) {
        S.generateComputeFunction(AddressModule);
      }
    }
  }

  // For debug perpose.
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(this->AddressModulePath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  AddressModule->print(ModuleFStream, nullptr);
  ModuleFStream.close();

  this->AddrInterpreter =
      std::make_unique<llvm::Interpreter>(std::move(AddressModule));
  this->FuncSE =
      std::make_unique<FunctionalStreamEngine>(this->AddrInterpreter);
}

StreamTransformPlan &
StreamPass::getOrCreatePlan(const llvm::Instruction *Inst) {
  auto UserPlanIter = this->InstPlanMap.find(Inst);
  if (UserPlanIter == this->InstPlanMap.end()) {
    UserPlanIter =
        this->InstPlanMap
            .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                     std::forward_as_tuple())
            .first;
  }
  return UserPlanIter->second;
}

StreamTransformPlan *
StreamPass::getPlanNullable(const llvm::Instruction *Inst) {
  auto UserPlanIter = this->InstPlanMap.find(Inst);
  if (UserPlanIter == this->InstPlanMap.end()) {
    return nullptr;
  }
  return &UserPlanIter->second;
}

void StreamPass::DEBUG_PLAN_FOR_LOOP(const llvm::Loop *Loop) {
  std::stringstream ss;
  ss << "DEBUG PLAN FOR LOOP " << LoopUtils::getLoopId(Loop)
     << "----------------\n";
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    ss << BB->getName().str() << "---------------------------\n";
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      std::string PlanStr = "NOTHING";
      if (auto PlanPtr = this->getPlanNullable(Inst)) {
        PlanStr = PlanPtr->format();
      }
      ss << std::setw(40) << std::left << LoopUtils::formatLLVMInst(Inst)
         << PlanStr << '\n';
    }
  }
  // llvm::errs() << ss.str() << '\n';

  // Also dump to file.
  std::string PlanPath = this->OutputExtraFolderPath + "/" +
                         LoopUtils::getLoopId(Loop) + ".plan.txt";
  std::ofstream PlanFStream(PlanPath);
  assert(PlanFStream.is_open() &&
         "Failed to open dump loop transform plan file.");
  PlanFStream << ss.str() << '\n';
  PlanFStream.close();
}

void StreamPass::DEBUG_SORTED_STREAMS_FOR_LOOP(const llvm::Loop *Loop) {
  auto Iter = this->ChosenLoopSortedStreams.find(Loop);
  DEBUG(llvm::errs() << "Dump sorted streams for loop "
                     << LoopUtils::getLoopId(Loop) << "-----------\n");
  if (Iter != this->ChosenLoopSortedStreams.end()) {
    for (auto S : Iter->second) {
      DEBUG(llvm::errs() << S->formatName() << '\n');
    }
  }
  DEBUG(llvm::errs() << "----------------------------\n");
}

void StreamPass::makeStreamTransformPlan() {

  /**
   * Slice the program and assign transformation plan for static instructions.
   * 1. Find the outer most loop O, for every stream S in (O union subloops(O)),
   *  a. Mark the stream as
   *    load -> delete, store -> stream-store, phi -> delete.
   *  b. Find the user of the stream and add user information.
   *  c. Find all the compute instructions and mark them as delete candidates.
   *  d. Mark address/phi instruction as deleted.
   *  e. Mark S as processed.
   * 2. Use the deleted instruction as the seed, and mark the delete candidates
   * as deleted iff. all users of the candidate are already marked as deleted.
   */
  std::unordered_set<const llvm::Loop *> ProcessedLoops;
  std::list<const llvm::Loop *> LoopBFSQueue;
  std::unordered_set<const llvm::Instruction *> DeleteCandidates;
  std::unordered_set<const llvm::Instruction *> DeletedInsts;
  std::list<const llvm::Instruction *> NewlyDeletedQueue;

  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {

    const llvm::Loop *OutMostLoop = LoopInstStreamEntry.first;
    if (ProcessedLoops.count(OutMostLoop) != 0) {
      continue;
    }

    while (OutMostLoop->getParentLoop() != nullptr) {
      OutMostLoop = OutMostLoop->getParentLoop();
    }

    // Process the OutMostLoop and all subloops.
    LoopBFSQueue.clear();
    DeleteCandidates.clear();
    DeletedInsts.clear();
    NewlyDeletedQueue.clear();
    LoopBFSQueue.emplace_back(OutMostLoop);
    while (!LoopBFSQueue.empty()) {
      auto Loop = LoopBFSQueue.front();
      LoopBFSQueue.pop_front();
      ProcessedLoops.insert(Loop);

      for (auto &SubLoop : Loop->getSubLoops()) {
        LoopBFSQueue.emplace_back(SubLoop);
      }

      auto ChosenLoopInstStreamIter = this->ChosenLoopInstStream.find(Loop);
      if (ChosenLoopInstStreamIter == this->ChosenLoopInstStream.end()) {
        // No stream is specialized at this loop.
        continue;
      }

      for (auto &InstStreamEntry : ChosenLoopInstStreamIter->second) {
        Stream *S = InstStreamEntry.second;
        DEBUG(llvm::errs() << "make transform plan for stream "
                           << S->formatName() << '\n');

        // Add all the compute instructions as delete candidates.
        for (const auto &ComputeInst : S->getComputeInsts()) {
          DeleteCandidates.insert(ComputeInst);
          DEBUG(llvm::errs() << "Compute instruction "
                             << LoopUtils::formatLLVMInst(ComputeInst) << '\n');
          if (Stream::isStepInst(ComputeInst) && S->hasNoBaseStream()) {
            /**
             * Mark a step instruction for streams that has no base stream.
             */
            this->getOrCreatePlan(ComputeInst).planToStep(S);
            DEBUG(llvm::errs()
                  << "Select transform plan for inst "
                  << LoopUtils::formatLLVMInst(ComputeInst) << " to "
                  << StreamTransformPlan::formatPlanT(
                         StreamTransformPlan::PlanT::STEP)
                  << '\n');
            /**
             * A hack here to make the user of step instruction also user of the
             * stream. PURE EVIL!!
             */
            for (auto U : ComputeInst->users()) {
              if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
                this->getOrCreatePlan(I).addUsedStream(S);
              }
            }
          }
        }

        // Add uses for all the users to use myself.
        auto &SelfInst = InstStreamEntry.first;
        for (auto U : SelfInst->users()) {
          if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
            this->getOrCreatePlan(I).addUsedStream(S);
            DEBUG(llvm::errs()
                  << "Add used stream for user " << LoopUtils::formatLLVMInst(I)
                  << " with stream " << S->formatName() << '\n');
          }
        }

        // Mark myself as DELETE (load) or STORE (store).
        auto &SelfPlan = this->getOrCreatePlan(SelfInst);
        if (llvm::isa<llvm::StoreInst>(SelfInst)) {
          assert(SelfPlan.Plan == StreamTransformPlan::PlanT::NOTHING &&
                 "Already have a plan for the store.");
          SelfPlan.planToStore(S);
        } else {
          // This is a load.
          SelfPlan.planToDelete();
        }
        // Self inst is always marked as deleted for later as a seed to find
        // deletable instructions. Even though a store instructions is not
        // really deleted.
        NewlyDeletedQueue.emplace_back(SelfInst);
        DEBUG(llvm::errs() << "Select transform plan for inst "
                           << LoopUtils::formatLLVMInst(SelfInst) << " to "
                           << StreamTransformPlan::formatPlanT(SelfPlan.Plan)
                           << '\n');
      }
    }

    // Second step: find deletable instruction from the candidates.
    while (!NewlyDeletedQueue.empty()) {
      auto NewlyDeletedInst = NewlyDeletedQueue.front();
      NewlyDeletedQueue.pop_front();
      if (DeletedInsts.count(NewlyDeletedInst) != 0) {
        // This has already been processed.
        continue;
      }
      // Actually mark this one as delete if we have no other plan for it.
      auto &Plan = this->getOrCreatePlan(NewlyDeletedInst);
      switch (Plan.Plan) {
      case StreamTransformPlan::PlanT::NOTHING:
      case StreamTransformPlan::PlanT::DELETE: {
        Plan.planToDelete();
        break;
      }
      }
      DEBUG(
          llvm::errs() << "Select transform plan for addr inst "
                       << LoopUtils::formatLLVMInst(NewlyDeletedInst) << " to "
                       << StreamTransformPlan::formatPlanT(Plan.Plan) << '\n');
      DeletedInsts.insert(NewlyDeletedInst);
      // Check all the operand.
      for (unsigned OperandIdx = 0,
                    NumOperands = NewlyDeletedInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto Operand = NewlyDeletedInst->getOperand(OperandIdx);
        if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
          if (DeleteCandidates.count(OperandInst) == 0) {
            continue;
          }
          if (DeletedInsts.count(OperandInst) != 0) {
            // This inst is already deleted.
            continue;
          }
          // This is a candidate, check whether all of its users are deleted.
          bool AllUsersDeleted = true;
          for (auto U : OperandInst->users()) {
            if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
              if (DeletedInsts.count(I) == 0) {
                AllUsersDeleted = false;
                break;
              }
            }
          }
          if (AllUsersDeleted) {
            // We can safely mark this one as newly deleted now.
            NewlyDeletedQueue.emplace_back(OperandInst);
          }
        }
      }
    }
    DEBUG(this->DEBUG_PLAN_FOR_LOOP(OutMostLoop));
  }
}

void StreamPass::sortChosenStream(Stream *S, std::list<Stream *> &Stack,
                                  std::unordered_set<Stream *> &SortedStreams) {
  for (auto PrevStream : Stack) {
    if (PrevStream == S) {
      DEBUG(llvm::errs() << "Recursive dependence found in stream. ");
      for (auto PrevStream : Stack) {
        DEBUG(llvm::errs() << PrevStream->formatName() << " -> ");
      }
      DEBUG(S->formatName());
      assert(false && "Recursion found in chosen stream dependence graph.");
    }
  }
  Stack.emplace_back(S);
  auto &InstStreamMap = this->ChosenLoopInstStream.at(S->getLoop());
  for (auto BaseStream : S->getBaseStreams()) {
    if (BaseStream == S) {
      // Dependent on myself is fine.
      continue;
    }
    if (InstStreamMap.count(BaseStream->getInst()) == 0) {
      // The base stream is not chosen at this level.
      continue;
    }
    if (SortedStreams.count(BaseStream) != 0) {
      continue;
    }
    this->sortChosenStream(BaseStream, Stack, SortedStreams);
  }
  auto Iter = this->ChosenLoopSortedStreams.find(S->getLoop());
  if (Iter == this->ChosenLoopSortedStreams.end()) {
    Iter = this->ChosenLoopSortedStreams
               .emplace(std::piecewise_construct,
                        std::forward_as_tuple(S->getLoop()),
                        std::forward_as_tuple())
               .first;
  }
  Iter->second.emplace_back(S);
  SortedStreams.insert(S);
  Stack.pop_back();
}

void StreamPass::pushLoopStackAndConfigureStreams(
    LoopStackT &LoopStack, llvm::Loop *NewLoop,
    DataGraph::DynamicInstIter NewInstIter,
    ActiveStreamInstMapT &ActiveStreamInstMap) {
  LoopStack.emplace_back(NewLoop);
  auto Iter = this->ChosenLoopSortedStreams.find(NewLoop);
  if (Iter == this->ChosenLoopSortedStreams.end()) {
    return;
  }
  if (Iter->second.empty()) {
    return;
  }

  auto NewDynamicInst = *NewInstIter;

  std::unordered_set<DynamicInstruction::DynamicId> InitDepIds;
  for (const auto &RegDep : this->Trace->RegDeps.at(NewDynamicInst->getId())) {
    InitDepIds.insert(RegDep.second);
  }

  for (auto &S : Iter->second) {

    // Inform the stream engine.
    this->FuncSE->configure(S, this->Trace);

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
    LoopStackT &LoopStack, ActiveStreamInstMapT &ActiveStreamInstMap) {
  assert(!LoopStack.empty() &&
         "Loop stack is empty when calling popLoopStackAndUnconfigureStreams.");
  auto EndedLoop = LoopStack.back();
  LoopStack.pop_back();

  auto Iter = this->ChosenLoopSortedStreams.find(EndedLoop);
  if (Iter == this->ChosenLoopSortedStreams.end()) {
    return;
  }
  if (Iter->second.empty()) {
    return;
  }

  for (auto &S : Iter->second) {
    auto Inst = S->getInst();
    auto ActiveStreamInstMapIter = ActiveStreamInstMap.find(Inst);
    assert(ActiveStreamInstMapIter != ActiveStreamInstMap.end() &&
           "The stream is not configured.");
    // ActiveStreamInstMap.erase(ActiveStreamInstMapIter);
    this->FuncSE->endStream(S);
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
      // if (DynamicInst->getId() > 19923000 && DynamicInst->getId() < 19923700)
      // {
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
        LoopStack.pop_back();
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
    if (PlanIter != this->InstPlanMap.end()) {
      const auto &TransformPlan = PlanIter->second;
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

        // Inform the functional stream engine.
        this->FuncSE->step(TransformPlan.getParamStream(), this->Trace);

        this->Trace->DynamicFrameStack.front()
            .updateRegisterDependenceLookUpMap(NewStaticInst,
                                               std::list<DynamicId>());

        auto NewDynamicId = NewDynamicInst->getId();
        delete NewDynamicInst;

        auto StepInst =
            new StreamStepInst(TransformPlan.getParamStream(), NewDynamicId);
        *NewInstIter = StepInst;
        NewDynamicInst = StepInst;

        /**
         * Handle the dependence for the step instruction.
         */
        auto StreamInst = TransformPlan.getParamStream()->getInst();
        auto StreamInstIter = ActiveStreamInstMap.find(StreamInst);
        if (StreamInstIter == ActiveStreamInstMap.end()) {
          ActiveStreamInstMap.emplace(StreamInst, NewDynamicId);
        } else {
          this->Trace->RegDeps.at(NewDynamicId)
              .emplace_back(nullptr, StreamInstIter->second);
          StreamInstIter->second = NewDynamicId;
        }

        this->StepInstCount++;
        NeedToHandleUseInformation = false;

      } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STORE) {

        assert(llvm::isa<llvm::StoreInst>(NewStaticInst) &&
               "STORE plan for non store instruction.");

        this->Trace->DynamicFrameStack.front()
            .updateRegisterDependenceLookUpMap(NewStaticInst,
                                               std::list<DynamicId>());

        /**
         * REPLACE the original store instruction with our special stream store.
         */
        auto NewDynamicId = NewDynamicInst->getId();
        delete NewDynamicInst;

        auto StoreInst =
            new StreamStoreInst(TransformPlan.getParamStream(), NewDynamicId);
        *NewInstIter = StoreInst;
        NewDynamicInst = StoreInst;
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
          auto UsedStreamInstIter = ActiveStreamInstMap.find(UsedStreamInst);
          if (UsedStreamInstIter != ActiveStreamInstMap.end()) {
            RegDeps.emplace_back(nullptr, UsedStreamInstIter->second);
          }
          for (auto &ChosenBaseStream : UsedStream->getAllChosenBaseStreams()) {
            auto Iter = ActiveStreamInstMap.find(ChosenBaseStream->getInst());
            if (Iter != ActiveStreamInstMap.end()) {
              RegDeps.emplace_back(nullptr, Iter->second);
            }
          }
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