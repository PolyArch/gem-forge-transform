
#include "MemoryAccessPattern.h"
#include "Replay.h"
#include "Utils.h"

#define DEBUG_TYPE "StreamPass"

namespace {

class Stream {
public:
  Stream(const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
         size_t _LoopLevel)
      : Inst(_Inst), Loop(_Loop), LoopLevel(_LoopLevel), TotalIters(0),
        TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
        StartId(DynamicInstruction::InvalidId), Pattern(_Inst) {
    assert(Utils::isMemAccessInst(this->Inst) &&
           "Should be load/store instruction to build a stream.");
    this->searchAddressComputeInstructions();
  }

  void addAccess(uint64_t Addr, DynamicInstruction::DynamicId DynamicId) {
    if (this->StartId == DynamicInstruction::InvalidId) {
      this->StartId = DynamicId;
    }
    this->LastAccessIters = this->Iters;
    this->Pattern.addAccess(Addr);
    this->TotalAccesses++;
  }

  void addMissingAccess() { this->Pattern.addMissingAccess(); }
  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->addMissingAccess();
    }
    this->Iters++;
    this->TotalIters;
  }

  void endStream() {
    this->Pattern.endStream();
    this->Iters = 1;
    this->LastAccessIters = 0;
    this->TotalStreams++;
    this->StartId = DynamicInstruction::InvalidId;
  }

  void addAliasInst(llvm::Instruction *AliasInst) {
    this->AliasInsts.insert(AliasInst);
  }

  void finalizePattern() { this->Pattern.finalizePattern(); }

  const llvm::Loop *getLoop() const { return this->Loop; }
  const llvm::Instruction *getInst() const { return this->Inst; }
  size_t getLoopLevel() const { return this->LoopLevel; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  DynamicInstruction::DynamicId getStartId() const { return this->StartId; }
  const MemoryAccessPattern &getPattern() const { return this->Pattern; }

  bool isIndirect() const { return !this->BaseLoads.empty(); }
  size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
  size_t getNumAddrInsts() const { return this->AddrInsts.size(); }
  const std::unordered_set<llvm::LoadInst *> &getBaseLoads() const {
    return this->BaseLoads;
  }
  const std::unordered_set<llvm::Instruction *> &getAliasInsts() const {
    return this->AliasInsts;
  }
  bool isAliased() const {
    // Check if we have alias with *other* instructions.
    for (const auto &AliasInst : this->AliasInsts) {
      if (AliasInst != this->Inst) {
        return true;
      }
    }
    return false;
  }

private:
  const llvm::Instruction *Inst;
  const llvm::Loop *Loop;
  const size_t LoopLevel;
  std::unordered_set<llvm::LoadInst *> BaseLoads;
  std::unordered_set<llvm::Instruction *> AddrInsts;
  std::unordered_set<llvm::Instruction *> AliasInsts;

  /**
   * Stores the total iterations for this stream
   */
  size_t TotalIters;
  size_t TotalAccesses;
  size_t TotalStreams;

  /**
   * Maintain the current iteration. Will be reset by endStream() and update by
   * endIter().
   * It should start at 1.
   */
  size_t Iters;
  /**
   * Maintain the iteration when the last addAccess() is called.
   * When endIter(), we check that LastAccessIters == Iters to detect missint
   * access in the last iteration.
   * It should be reset to 0 (should be less than reset value of Iters).
   */
  size_t LastAccessIters;

  /**
   * Stores the dynamic id of the first access in the current stream.
   */
  DynamicInstruction::DynamicId StartId;
  MemoryAccessPattern Pattern;

  void searchAddressComputeInstructions();
};

class StreamPass : public ReplayTrace {
public:
  static char ID;
  StreamPass() : ReplayTrace(ID), DynInstCount(0), DynMemInstCount(0) {}

protected:
  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;
  void dumpStats(std::ostream &O) override;
  void transform() override;

  std::string classifyStream(const Stream &S) const;
  using ActiveStreamMapT =
      std::unordered_map<const llvm::Loop *,
                         std::unordered_map<llvm::Instruction *, Stream *>>;
  using LoopStackT = std::list<llvm::Loop *>;

  void analyzeStream();
  bool isLoopContinuous(const llvm::Loop *Loop);
  void addAccess(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                 DynamicInstruction *DynamicInst);

  void handleAlias(DynamicInstruction *DynamicInst);

  void endIter(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams);
  void pushLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                     llvm::Loop *NewLoop);
  void popLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams);
  void activateStream(ActiveStreamMapT &ActiveStreams,
                      llvm::Instruction *Instruction);
  void initializeStreamIfNecessary(const LoopStackT &LoopStack,
                                   llvm::Instruction *Inst);

  void chooseStream();

  std::unordered_map<llvm::Instruction *, std::list<Stream>> InstStreamMap;

  std::unordered_map<llvm::Loop *,
                     std::unordered_map<llvm::Instruction *, Stream *>>
      ChosenLoopInstStreamMap;

  std::unordered_map<const llvm::Loop *,
                     std::unordered_set<llvm::Instruction *>>
      MemorizedMemoryAccess;
  std::unordered_map<const llvm::Loop *, bool> MemorizedLoopContinuous;

  std::unordered_map<llvm::Instruction *, uint64_t> MemAccessInstCount;

  /*****************************************
   * Statistics.
   */
  uint64_t DynInstCount;
  uint64_t DynMemInstCount;
};

bool StreamPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);
  this->MemorizedMemoryAccess.clear();
  return Ret;
}

bool StreamPass::finalize(llvm::Module &Module) {
  return ReplayTrace::finalize(Module);
}

std::string StreamPass::classifyStream(const Stream &S) const {
  const auto &Loop = S.getLoop();

  if (S.getNumBaseLoads() > 1) {
    return "MULTI_BASE";
  } else if (S.getNumBaseLoads() == 1) {
    const auto &BaseLoad = *S.getBaseLoads().begin();
    if (BaseLoad == S.getInst()) {
      // Chasing myself.
      return "POINTER_CHASE";
    }
    auto BaseStreamsIter = this->InstStreamMap.find(BaseLoad);
    if (BaseStreamsIter != this->InstStreamMap.end()) {
      const auto &BaseStreams = BaseStreamsIter->second;
      for (const auto &BaseStream : BaseStreams) {
        if (BaseStream.getLoop() == S.getLoop()) {
          if (BaseStream.getNumBaseLoads() != 0) {
            return "CHAIN_BASE";
          }
          if (BaseStream.getPattern().computed()) {
            auto Pattern = BaseStream.getPattern().getPattern().CurrentPattern;
            if (Pattern <= MemoryAccessPattern::Pattern::QUARDRIC) {
              return "AFFINE_BASE";
            }
          }
        }
      }
    }
    return "RANDOM_BASE";
  } else {
    if (S.getPattern().computed()) {
      auto Pattern = S.getPattern().getPattern().CurrentPattern;
      if (Pattern <= MemoryAccessPattern::Pattern::QUARDRIC) {
        return "AFFINE";
      } else {
        return "RANDOM";
      }
    } else {
      return "RANDOM";
    }
  }
}

void StreamPass::dumpStats(std::ostream &O) {

  O << "--------------- Stats -----------------\n";
#define print(value) (O << #value << ": " << this->value << '\n')
  print(DynInstCount);
  print(DynMemInstCount);
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
   * Chosen
   */

  O << "--------------- Stream -----------------\n";
  for (auto &InstStreamEntry : this->InstStreamMap) {
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
      size_t Footprint = 0;
      size_t AddrInsts = Stream.getNumAddrInsts();

      std::string Chosen = "NO";
      // auto Iter =
      // this->MemInstToChosenStreamMap.find(MemInstStreamEntry.first); if (Iter
      // != this->MemInstToChosenStreamMap.end()) {
      //   if (Iter->second == &Stream) {
      //     Chosen = "YES";
      //   }
      // }

      size_t AliasInsts = Stream.getAliasInsts().size();

      if (Stream.getPattern().computed()) {
        const auto &Pattern = Stream.getPattern().getPattern();
        AddrPattern =
            MemoryAccessPattern::formatPattern(Pattern.CurrentPattern);
        AccPattern =
            MemoryAccessPattern::formatAccessPattern(Pattern.AccPattern);
        Updates = Pattern.Updates;
        Footprint =
            Stream.getPattern().getFootprint().getNumCacheLinesAccessed();
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
      O << ' ' << Chosen;
      O << '\n';
    }
  }

  // Also dump statistics for all the unanalyzed memory accesses.
  size_t TotalAccesses = 0;
  for (const auto &Entry : this->MemAccessInstCount) {
    const auto &Inst = Entry.first;
    TotalAccesses += Entry.second;
    if (this->InstStreamMap.count(Inst) != 0) {
      // This instruction is already dumped.
      if (!this->InstStreamMap.at(Inst).empty()) {
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
    O << ' ' << "NO" << '\n';                    // Chosen
  }

  assert(TotalAccesses == this->DynMemInstCount && "Mismatch total accesses.");

  O << "--------------- Loop -----------------\n";
}

void StreamPass::transform() {
  this->analyzeStream();
  for (auto &InstStreamEntry : this->InstStreamMap) {
    for (auto &Stream : InstStreamEntry.second) {
      Stream.finalizePattern();
    }
  }
}

void StreamPass::initializeStreamIfNecessary(const LoopStackT &LoopStack,
                                             llvm::Instruction *Inst) {
  {
    // Initialize the global count.
    auto Iter = this->MemAccessInstCount.find(Inst);
    if (Iter == this->MemAccessInstCount.end()) {
      this->MemAccessInstCount.emplace(Inst, 0);
    }
  }

  auto Iter = this->InstStreamMap.find(Inst);
  if (Iter == this->InstStreamMap.end()) {
    Iter = this->InstStreamMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                        std::forward_as_tuple())
               .first;
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
  while (LoopIter != LoopStack.rend()) {
    auto LoopLevel = Streams.size();
    Streams.emplace_back(Inst, *LoopIter, LoopLevel);
    ++LoopIter;
  }
}

void StreamPass::activateStream(ActiveStreamMapT &ActiveStreams,
                                llvm::Instruction *Inst) {

  auto InstStreamMapIter = this->InstStreamMap.find(Inst);
  assert(InstStreamMapIter != this->InstStreamMap.end() &&
         "Failed to look up streams to activate.");

  for (auto &S : InstStreamMapIter->second) {
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

void StreamPass::pushLoopStack(LoopStackT &LoopStack,
                               ActiveStreamMapT &ActiveStreams,
                               llvm::Loop *Loop) {

  LoopStack.emplace_back(Loop);

  auto Iter = this->MemorizedMemoryAccess.find(Loop);
  if (Iter == this->MemorizedMemoryAccess.end()) {
    Iter = this->MemorizedMemoryAccess
               .emplace(Loop, std::unordered_set<llvm::Instruction *>())
               .first;
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

        Iter->second.insert(StaticInst);
      }
    }
  }

  for (auto StaticInst : Iter->second) {
    this->initializeStreamIfNecessary(LoopStack, StaticInst);
    this->activateStream(ActiveStreams, StaticInst);
  }
}

void StreamPass::addAccess(const LoopStackT &LoopStack,
                           ActiveStreamMapT &ActiveStreams,
                           DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  this->initializeStreamIfNecessary(LoopStack, StaticInst);
  this->MemAccessInstCount.at(StaticInst)++;
  this->activateStream(ActiveStreams, StaticInst);
  auto Iter = this->InstStreamMap.find(StaticInst);
  if (Iter == this->InstStreamMap.end()) {
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
  auto Iter = this->InstStreamMap.find(StaticInst);
  if (Iter == this->InstStreamMap.end()) {
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
                         ActiveStreamMapT &ActiveStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when endIter is called.");
  const auto &EndedLoop = LoopStack.back();
  auto ActiveStreamsIter = ActiveStreams.find(EndedLoop);
  if (ActiveStreamsIter != ActiveStreams.end()) {
    for (auto &InstStreamEntry : ActiveStreamsIter->second) {
      auto &Stream = InstStreamEntry.second;
      assert(Stream->getLoop() == EndedLoop &&
             "Mismatch on loop for streams in ActiveStreams.");
      Stream->endIter();
    }
  }
}

void StreamPass::popLoopStack(LoopStackT &LoopStack,
                              ActiveStreamMapT &ActiveStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when popLoopStack is called.");
  this->endIter(LoopStack, ActiveStreams);

  // Deactivate all the streams in this loop level.
  const auto &EndedLoop = LoopStack.back();
  auto ActiveStreamsIter = ActiveStreams.find(EndedLoop);
  if (ActiveStreamsIter != ActiveStreams.end()) {
    for (auto &InstStreamEntry : ActiveStreamsIter->second) {
      InstStreamEntry.second->endStream();
    }
    ActiveStreams.erase(ActiveStreamsIter);
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

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeadOfCandidate = false;
    bool IsMemAccess = false;

    while (this->Trace->DynamicInstructionList.size() > 100000) {
      this->Trace->commitOneDynamicInst();
    }

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewInstIter)->getStaticInstruction();
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
        this->popLoopStack(LoopStack, ActiveStreams);
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
      this->popLoopStack(LoopStack, ActiveStreams);
    }

    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back() != NewLoop) {
        // A new loop.
        this->pushLoopStack(LoopStack, ActiveStreams, NewLoop);
      } else {
        // This means that we are at a new iteration.
        this->endIter(LoopStack, ActiveStreams);
      }
    }

    if (IsMemAccess) {
      this->DynMemInstCount++;
      this->addAccess(LoopStack, ActiveStreams, *NewInstIter);
      this->handleAlias(*NewInstIter);
      this->CacheWarmerPtr->addAccess(Utils::getMemAddr(*NewInstIter));
    }
  }
}

void Stream::searchAddressComputeInstructions() {

  std::list<llvm::Instruction *> Queue;

  llvm::Value *AddrValue = nullptr;
  if (llvm::isa<llvm::LoadInst>(this->Inst)) {
    AddrValue = this->Inst->getOperand(0);
  } else {
    AddrValue = this->Inst->getOperand(1);
  }

  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(AddrValue)) {
    Queue.emplace_back(AddrInst);
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    if (this->AddrInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      continue;
    }
    if (!this->Loop->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    this->AddrInsts.insert(CurrentInst);
    if (auto BaseLoad = llvm::dyn_cast<llvm::LoadInst>(CurrentInst)) {
      // This is also a base load.
      this->BaseLoads.insert(BaseLoad);
      // Do not go further for load.
      continue;
    }

    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        // This is an instruction within the same context.
        Queue.emplace_back(OperandInst);
      }
    }
  }
}

void StreamPass::chooseStream() {
  /**
   * First choose direct streams that
   * 1. Is affine.
   * 2. Has no alias.
   * 3. At highest loop level.
   */
  for (const auto &InstStreamEntry : this->InstStreamMap) {
    const auto &Streams = InstStreamEntry.second;
    for (const auto &S : Streams) {
      if (S.isAliased()) {
        // No need to look higher.
        break;
      }
      if (S.isIndirect()) {
        // Indirect streams are handled later.
        break;
      }
      const auto &MemPattern = S.getPattern();
      if (!MemPattern.computed()) {
        // Ignore those uncomputed streams.
        break;
      }
    }
  }
}

} // namespace

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);