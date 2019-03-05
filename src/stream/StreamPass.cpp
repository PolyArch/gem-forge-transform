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
  this->AddressModulePath = this->OutputExtraFolderPath + "/stream.addr.ll";
  this->MemorizedStreamInst.clear();
  return Ret;
}

bool StreamPass::finalize(llvm::Module &Module) {
  return ReplayTrace::finalize(Module);
}

std::string StreamPass::classifyStream(const MemStream &S) const {

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
  auto BaseIVStream = this->getIVStreamByPHINodeLoop(BaseIV, S.getLoop());
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
   * PossiblePaths
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

      int LoopPossiblePaths =
          this->MemorizedLoopPossiblePaths.at(Stream.getLoop());

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
    O << ' ' << "NO";                            // Chosen
    O << ' ' << 1;                               // PossiblePaths
    O << '\n';
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

      int LoopPossiblePaths =
          this->MemorizedLoopPossiblePaths.at(IVStream.getLoop());

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
}

void StreamPass::transform() {
  this->analyzeStream();
  // this->buildChosenStreamDependenceGraph();
  // this->buildAllChosenStreamDependenceGraph();

  // this->buildAddressInterpreterForChosenStreams();

  // this->makeStreamTransformPlan();

  // for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
  //   std::unordered_set<Stream *> SortedStreams;
  //   for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
  //     auto S = InstStreamEntry.second;
  //     if (SortedStreams.count(S) != 0) {
  //       continue;
  //     }
  //     std::list<Stream *> Stack;
  //     this->sortChosenStream(S, Stack, SortedStreams);
  //   }
  //   DEBUG(this->DEBUG_SORTED_STREAMS_FOR_LOOP(LoopInstStreamEntry.first));
  // }

  // delete this->Trace;
  // this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  // this->transformStream();

  // /**
  //  * Add the coalesce info.
  //  */
  // this->FuncSE->finalizeCoalesceInfo();

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

MemStream *StreamPass::getMemStreamByInstLoop(const llvm::Instruction *Inst,
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

InductionVarStream *
StreamPass::getIVStreamByPHINodeLoop(const llvm::PHINode *PHINode,
                                     const llvm::Loop *Loop) {
  auto Iter = this->PHINodeIVStreamMap.find(PHINode);
  if (Iter == this->PHINodeIVStreamMap.end()) {
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

const InductionVarStream *
StreamPass::getIVStreamByPHINodeLoop(const llvm::PHINode *PHINode,
                                     const llvm::Loop *Loop) const {
  auto Iter = this->PHINodeIVStreamMap.find(PHINode);
  if (Iter == this->PHINodeIVStreamMap.end()) {
    return nullptr;
  }
  const auto &Streams = Iter->second;
  for (const auto &S : Streams) {
    if (S.getLoop() == Loop) {
      return &S;
    }
  }
  llvm_unreachable("Failed to find the stream at specified loop level.");
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
        DEBUG(llvm::errs() << "Generate address compute function for stream "
                           << S.formatName() << '\n');
        S.generateComputeFunction(AddressModule);
      }
    }
  }

  // For debug perpose.
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(this->AddressModulePath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  llvm::outs() << "AddressModulePath " << this->AddressModulePath << '\n';
  assert(!ModuleFStream.has_error() &&
         "Failed to open the address computation module file.");
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

void StreamPass::dumpInfoForLoop(const llvm::Loop *Loop, std::ostream &OS,
                                 const std::string &Padding) const {
#define INFO_OUT (OS << Padding)

  auto ParentLoop = Loop->getParentLoop();
  std::string ParentLoopId = "None";
  if (ParentLoop != nullptr) {
    ParentLoopId = LoopUtils::getLoopId(ParentLoop);
  }
  INFO_OUT << LoopUtils::getLoopId(Loop) << ' ' << ParentLoopId << '\n';
  auto SortedChosenStreamsIter = this->ChosenLoopSortedStreams.find(Loop);
  if (SortedChosenStreamsIter != this->ChosenLoopSortedStreams.end()) {
    for (const auto &S : SortedChosenStreamsIter->second) {
      INFO_OUT << S->formatName() << ' ' << S->getCoalesceGroup() << '\n';
    }
  }

  std::string SubLoopPadding = Padding + "  ";
  for (const auto &SubLoop : Loop->getSubLoops()) {
    this->dumpInfoForLoop(SubLoop, OS, SubLoopPadding);
  }

#undef INFO_OUT
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
      ss << std::setw(50) << std::left << LoopUtils::formatLLVMInst(Inst)
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
   * 2. Use the deleted dependence edge as the seed, and mark the delete
   * candidates as deleted iff. all the use edges of the candidate are already
   * marked as deleted.
   */
  std::unordered_set<const llvm::Loop *> ProcessedLoops;
  std::list<const llvm::Loop *> LoopBFSQueue;
  std::unordered_set<const llvm::Instruction *> DeleteCandidates;
  std::unordered_set<const llvm::Instruction *> DeletedInsts;
  std::list<const llvm::Use *> NewlyDeletingQueue;
  std::unordered_set<const llvm::Use *> DeletedUses;
  /**
   * Initialize the transform plan for all the instructions
   * in the loop.
   */
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
    auto Loop = LoopInstStreamEntry.first;
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        this->getOrCreatePlan(&*InstIter);
      }
    }
  }

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
    NewlyDeletingQueue.clear();
    DeletedUses.clear();
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

        // Handle all the step instructions.
        if (S->Type == Stream::TypeT::IV) {
          for (const auto &StepInst : S->getStepInsts()) {
            this->getOrCreatePlan(StepInst).planToStep(S);
            DEBUG(llvm::errs() << "Select transform plan for inst "
                               << LoopUtils::formatLLVMInst(StepInst) << " to "
                               << StreamTransformPlan::formatPlanT(
                                      StreamTransformPlan::PlanT::STEP)
                               << '\n');
            /**
             * A hack here to make the user of step instruction also user of the
             * stream. PURE EVIL!!
             * TODO: Is there better way to do this?
             */
            for (auto U : StepInst->users()) {
              if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
                this->getOrCreatePlan(I).addUsedStream(S);
              }
            }
          }
        }

        // Add all the compute instructions as delete candidates.
        for (const auto &ComputeInst : S->getComputeInsts()) {
          DeleteCandidates.insert(ComputeInst);
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
          // For store, only the address use is deleted.
          NewlyDeletingQueue.push_back(&SelfInst->getOperandUse(1));
        } else {
          // For load or iv, delete if we have no other plan for it.
          if (SelfPlan.Plan == StreamTransformPlan::PlanT::NOTHING) {
            SelfPlan.planToDelete();
          }
          for (unsigned OperandIdx = 0,
                        NumOperands = SelfInst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            NewlyDeletingQueue.push_back(&SelfInst->getOperandUse(OperandIdx));
          }
        }

        DEBUG(llvm::errs() << "Select transform plan for inst "
                           << LoopUtils::formatLLVMInst(SelfInst) << " to "
                           << StreamTransformPlan::formatPlanT(SelfPlan.Plan)
                           << '\n');
      }
    }

    // Second step: find deletable instruction from the candidates.
    while (!NewlyDeletingQueue.empty()) {
      auto NewlyDeletingUse = NewlyDeletingQueue.front();
      NewlyDeletingQueue.pop_front();

      if (DeletedUses.count(NewlyDeletingUse) != 0) {
        // We have already process this use.
        continue;
      }

      DeletedUses.insert(NewlyDeletingUse);

      auto NewlyDeletingValue = NewlyDeletingUse->get();
      if (auto NewlyDeletingOperandInst =
              llvm::dyn_cast<llvm::Instruction>(NewlyDeletingValue)) {
        // The operand of the deleting use is an instruction.
        if (DeleteCandidates.count(NewlyDeletingOperandInst) == 0) {
          // The operand is not even a delete candidate.
          continue;
        }
        if (DeletedInsts.count(NewlyDeletingOperandInst) != 0) {
          // This inst is already deleted.
          continue;
        }
        // Check if all the uses have been deleted.
        bool AllUsesDeleted = true;
        for (const auto &U : NewlyDeletingOperandInst->uses()) {
          if (DeletedUses.count(&U) == 0) {
            AllUsesDeleted = false;
            break;
          }
        }
        if (AllUsesDeleted) {
          // We can safely delete this one now.
          // Actually mark this one as delete if we have no other plan for it.
          auto &Plan = this->getOrCreatePlan(NewlyDeletingOperandInst);
          switch (Plan.Plan) {
          case StreamTransformPlan::PlanT::NOTHING:
          case StreamTransformPlan::PlanT::DELETE: {
            Plan.planToDelete();
            break;
          }
          }
          // Add all the uses to NewlyDeletingQueue.
          for (unsigned
                   OperandIdx = 0,
                   NumOperands = NewlyDeletingOperandInst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            NewlyDeletingQueue.push_back(
                &NewlyDeletingOperandInst->getOperandUse(OperandIdx));
          }
        }
      }
    }
    this->DEBUG_PLAN_FOR_LOOP(OutMostLoop);
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
    LoopStackT &LoopStack, DataGraph::DynamicInstIter NewInstIter,
    ActiveStreamInstMapT &ActiveStreamInstMap) {
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

  std::unordered_set<DynamicInstruction::DynamicId> InitDepIds;

  if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
    auto NewDynamicInst = *NewInstIter;
    for (const auto &RegDep :
         this->Trace->RegDeps.at(NewDynamicInst->getId())) {
      InitDepIds.insert(RegDep.second);
    }
  }

  for (auto &S : Iter->second) {
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
    if (llvm::isa<llvm::LoadInst>(NewStaticInst)) {
      auto ChosenInstStreamIter = this->ChosenInstStreamMap.find(NewStaticInst);
      if (ChosenInstStreamIter != this->ChosenInstStreamMap.end()) {
        auto S = ChosenInstStreamIter->second;
        this->FuncSE->updateLoadedValue(S, this->Trace,
                                        *(NewDynamicInst->DynamicResult));
      }
    }

    // Update the phi node value for functional stream engine.
    for (unsigned OperandIdx = 0, NumOperands = NewStaticInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = NewStaticInst->getOperand(OperandIdx);
      if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {

        auto ChosenInstStreamIter = this->ChosenInstStreamMap.find(PHINode);
        if (ChosenInstStreamIter == this->ChosenInstStreamMap.end()) {
          // This is not a phi node we are about.
          continue;
        }

        auto S = ChosenInstStreamIter->second;
        this->FuncSE->updatePHINodeValue(
            S, this->Trace, *(NewDynamicInst->DynamicOperands[OperandIdx]));
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

        /**
         * A step transform plan means that this instructions is deleted, and
         * one or more step instructions are inserted.
         */

        // Insert all the step instructions.
        for (auto StepStream : TransformPlan.getStepStreams()) {
          // Inform the functional stream engine that this stream stepped.
          this->FuncSE->step(StepStream, this->Trace);

          // Create the new StepInst.
          auto StepInst = new StreamStepInst(StepStream);
          auto StepInstId = StepInst->getId();

          this->Trace->insertDynamicInst(NewInstIter, StepInst);

          /**
           * Handle the dependence for the step instruction.
           * Step inst should also wait for the dependent stream insts.
           */
          auto &RegDeps = this->Trace->RegDeps.at(StepInstId);
          for (const auto &AllChosenDependentStream :
               StepStream->getAllChosenDependentStreams()) {
            auto StreamInst = AllChosenDependentStream->getInst();
            auto StreamInstIter = ActiveStreamInstMap.find(StreamInst);
            // Also register myself as the latest stream inst for the dependent
            // streams.
            if (StreamInstIter == ActiveStreamInstMap.end()) {
              ActiveStreamInstMap.emplace(StreamInst, StepInstId);
            } else {
              RegDeps.emplace_back(nullptr, StreamInstIter->second);
              StreamInstIter->second = StepInstId;
            }
          }
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
         * REPLACE the original store instruction with our special stream store.
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
            this->FuncSE->access(UsedStream);
            auto UsedStreamInstIter = ActiveStreamInstMap.find(UsedStreamInst);
            if (UsedStreamInstIter != ActiveStreamInstMap.end()) {
              RegDeps.emplace_back(nullptr, UsedStreamInstIter->second);
            }
            for (auto &ChosenBaseStream :
                 UsedStream->getAllChosenBaseStreams()) {
              auto Iter = ActiveStreamInstMap.find(ChosenBaseStream->getInst());
              if (Iter != ActiveStreamInstMap.end()) {
                RegDeps.emplace_back(nullptr, Iter->second);
              }
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
          this->FuncSE->access(UsedStream);
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
