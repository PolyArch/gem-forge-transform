
#include "Replay.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"
#include "stream/MemStream.h"

#include <sstream>

#define DEBUG_TYPE "StreamPass"

namespace {

struct StreamTransformPlan {
public:
  enum PlanT {
    NOTHING,
    DELETE,
    STEP,
    STORE,
  } Plan;

  static std::string formatPlanT(const PlanT Plan) {
    switch (Plan) {
    case NOTHING:
      return "NOTHING";
    case DELETE:
      return "DELETE";
    case STEP:
      return "STEP";
    case STORE:
      return "STORE";
    }
    llvm_unreachable("Illegal StreamTransformPlan::PlanT to be formatted.");
  }

  StreamTransformPlan() : Plan(NOTHING) {}

  const std::unordered_set<Stream *> &getUsedStreams() const {
    return this->UsedStreams;
  }
  Stream *getParamStream() const { return this->ParamStream; }

  void addUsedStream(Stream *UsedStream) {
    this->UsedStreams.insert(UsedStream);
  }

  void planToDelete() { this->Plan = DELETE; }
  void planToStep(Stream *S) {
    this->ParamStream = S;
    this->Plan = STEP;
  }
  void planToStore(Stream *S) {
    this->ParamStream = S;
    this->Plan = STORE;
  }

private:
  std::unordered_set<Stream *> UsedStreams;
  Stream *ParamStream;
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

  std::string classifyStream(const MemStream &S) const;
  using ActiveStreamMapT =
      std::unordered_map<const llvm::Loop *,
                         std::unordered_map<llvm::Instruction *, MemStream *>>;
  using ActiveIVStreamMapT = std::unordered_map<
      const llvm::Loop *,
      std::unordered_map<const llvm::PHINode *, InductionVarStream *>>;
  using LoopStackT = std::list<llvm::Loop *>;

  /*************************************************************
   * Stream Analysis.
   *************************************************************/

  void analyzeStream();
  bool isLoopContinuous(const llvm::Loop *Loop);
  void addAccess(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                 DynamicInstruction *DynamicInst);

  void handleAlias(DynamicInstruction *DynamicInst);

  void endIter(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
               ActiveIVStreamMapT &ActiveIVStreams);
  void pushLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                     ActiveIVStreamMapT &ActiveIVStreams, llvm::Loop *NewLoop);
  void popLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                    ActiveIVStreamMapT &ActiveIVStreams);
  void activateStream(ActiveStreamMapT &ActiveStreams,
                      llvm::Instruction *Instruction);
  void activateIVStream(ActiveIVStreamMapT &ActiveIVStreams,
                        llvm::PHINode *PHINode);
  void initializeMemStreamIfNecessary(const LoopStackT &LoopStack,
                                      llvm::Instruction *Inst);
  void initializeIVStreamIfNecessary(const LoopStackT &LoopStack,
                                     llvm::PHINode *Inst);

  /*************************************************************
   * Stream Choice.
   *************************************************************/
  void chooseStream();
  void buildStreamDependenceGraph();
  void markQualifiedStream();
  void addChosenStream(const llvm::Loop *Loop, const llvm::Instruction *Inst,
                       Stream *S);
  MemStream *getMemStreamByInstLoop(llvm::Instruction *Inst,
                                    const llvm::Loop *Loop);

  /*************************************************************
   * Stream transform.
   *************************************************************/
  using StreamUserMapT =
      std::unordered_map<Stream *,
                         std::unordered_set<DynamicInstruction::DynamicId>>;
  void makeStreamTransformPlan();
  void addUserToStream(DynamicInstruction::DynamicId, Stream *S,
                       StreamUserMapT &StreamUsers);
  void transformStream();

  std::unordered_map<llvm::Instruction *, std::list<MemStream>>
      InstMemStreamMap;

  std::unordered_map<const llvm::PHINode *, std::list<InductionVarStream>>
      PHINodeIVStreamMap;

  std::unordered_map<const llvm::Loop *,
                     std::unordered_map<const llvm::Instruction *, Stream *>>
      ChosenLoopInstStream;

  std::unordered_map<const llvm::Instruction *, StreamTransformPlan>
      InstPlanMap;

  /************************************************************
   * Memorization.
   ************************************************************/
  std::unordered_set<const llvm::Loop *> InitializedLoops;
  std::unordered_map<const llvm::Loop *,
                     std::unordered_set<llvm::Instruction *>>
      MemorizedStreamInst;
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
            auto Pattern = BaseStream.getPattern().getPattern().AddrPattern;
            if (Pattern <= MemoryPattern::AddressPattern::QUARDRIC) {
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
            auto Pattern = BaseIVStream.getPattern().getPattern().AddrPattern;
            if (Pattern <= MemoryPattern::AddressPattern::QUARDRIC) {
              return "AFFINE_IV";
            }
          }
        }
      }
      return "RANDOM_IV";
    } else {
      if (S.getPattern().computed()) {
        auto Pattern = S.getPattern().getPattern().AddrPattern;
        if (Pattern <= MemoryPattern::AddressPattern::QUARDRIC) {
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
        AddrPattern = MemoryPattern::formatAddressPattern(Pattern.AddrPattern);
        AccPattern = MemoryPattern::formatAccessPattern(Pattern.AccPattern);
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
        AddrPattern = MemoryPattern::formatAddressPattern(Pattern.AddrPattern);
        AccPattern = MemoryPattern::formatAccessPattern(Pattern.AccPattern);
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
  for (auto &InstStreamEntry : this->InstMemStreamMap) {
    for (auto &Stream : InstStreamEntry.second) {
      Stream.finalizePattern();
    }
  }
  this->chooseStream();

  this->makeStreamTransformPlan();

  // delete this->Trace;
  // this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  // this->transformStream();
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
    return this->PHINodeIVStreamMap.count(PHINode) != 0;
    // return false;
  };
  while (LoopIter != LoopStack.rend()) {
    auto LoopLevel = Streams.size();
    Streams.emplace_back(this->OutputExtraFolderPath, Inst, *LoopIter,
                         LoopLevel, IsInductionVar);
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
    auto ComputeInsts = InductionVarStream::searchComputeInsts(Inst, Loop);
    if (InductionVarStream::isInductionVarStream(Inst, ComputeInsts)) {
      Streams.emplace_back(this->OutputExtraFolderPath, Inst, Loop, Level,
                           std::move(ComputeInsts));
      DEBUG(llvm::errs() << "Initialize IVStream "
                         << Streams.back().formatName() << '\n');
      ++LoopIter;
    }
  }
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
    }
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
      auto AddrPattern = Pattern.getPattern().AddrPattern;
      if (AddrPattern <= MemoryPattern::AddressPattern::QUARDRIC) {
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
      auto AddrPattern = MemPattern.getPattern().AddrPattern;
      if (AddrPattern <= MemoryPattern::AddressPattern::QUARDRIC) {
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
  DEBUG(llvm::errs() << "Add chosen stream " << S->formatName() << '\n');
  ChosenLoopInstStreamIter->second.emplace(Inst, S);
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

void StreamPass::makeStreamTransformPlan() {
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStream) {
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      Stream *S = InstStreamEntry.second;

      DEBUG(llvm::errs() << "make transform plan for stream " << S->formatName()
                         << '\n');

      for (const auto &ComputeInst : S->getComputeInsts()) {
        // For now simply mark them as delete.
        auto PlanIter = this->InstPlanMap.find(ComputeInst);
        if (PlanIter == this->InstPlanMap.end()) {
          PlanIter = this->InstPlanMap
                         .emplace(std::piecewise_construct,
                                  std::forward_as_tuple(ComputeInst),
                                  std::forward_as_tuple())
                         .first;
        }
        switch (PlanIter->second.Plan) {
        case StreamTransformPlan::PlanT::NOTHING:
        case StreamTransformPlan::PlanT::DELETE: {
          PlanIter->second.planToDelete();
          break;
        }
        }
        if (Stream::isStepInst(ComputeInst) && S->hasNoBaseStream()) {
          /**
           * Mark a step instruction for streams that has no base stream.
           */
          PlanIter->second.planToStep(S);
        }
        DEBUG(llvm::errs() << "Select transform plan for addr inst "
                           << LoopUtils::formatLLVMInst(ComputeInst) << " to "
                           << StreamTransformPlan::formatPlanT(
                                  PlanIter->second.Plan)
                           << '\n');
      }

      // Add uses for all the users to use myself.
      auto &Inst = InstStreamEntry.first;
      for (auto U : Inst->users()) {
        if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
          auto UserPlanIter = this->InstPlanMap.find(I);
          if (UserPlanIter == this->InstPlanMap.end()) {
            UserPlanIter =
                this->InstPlanMap
                    .emplace(std::piecewise_construct, std::forward_as_tuple(I),
                             std::forward_as_tuple())
                    .first;
          }
          UserPlanIter->second.addUsedStream(S);
          DEBUG(llvm::errs()
                << "Add used stream for user " << LoopUtils::formatLLVMInst(I)
                << " with stream " << S->formatName() << '\n');
        }
      }

      // Mark myself as DELETE (load) or STORE (store).
      auto InstPlanIter =
          this->InstPlanMap
              .emplace(std::piecewise_construct, std::forward_as_tuple(Inst),
                       std::forward_as_tuple())
              .first;
      if (llvm::isa<llvm::StoreInst>(Inst)) {
        assert(InstPlanIter->second.Plan ==
                   StreamTransformPlan::PlanT::NOTHING &&
               "Already have a plan for the store.");
        InstPlanIter->second.planToStore(S);
      } else {
        // This is a load.
        InstPlanIter->second.planToDelete();
      }
      DEBUG(llvm::errs() << "Select transform plan for inst "
                         << LoopUtils::formatLLVMInst(Inst) << " to "
                         << StreamTransformPlan::formatPlanT(
                                InstPlanIter->second.Plan)
                         << '\n');
    }
  }
}

class StreamStepInst : public DynamicInstruction {
public:
  StreamStepInst() = default;
  std::string getOpName() const override { return "stream-step"; }
};

class StreamStoreInst : public DynamicInstruction {
public:
  StreamStoreInst(DynamicId _Id) : DynamicInstruction() {
    /**
     * Inherit the provided dynamic id.
     */
    this->Id = _Id;
  }
  std::string getOpName() const override { return "stream-store"; }
};

class StreamConfigInst : public DynamicInstruction {};

void StreamPass::transformStream() {
  DEBUG(llvm::errs() << "Stream: Start transform.\n");

  LoopStackT LoopStack;

  while (true) {
    auto NewInstIter = this->Trace->loadOneDynamicInst();

    DynamicInstruction *NewDynamicInst = nullptr;
    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeadOfCandidate = false;

    while (this->Trace->DynamicInstructionList.size() > 10) {
      this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                  this->Trace);
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
      LoopStack.pop_back();
    }

    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back() != NewLoop) {
        // A new loop. We should configure all the streams here.
      } else {
        // This means that we are at a new iteration.
      }
    }

    auto PlanIter = this->InstPlanMap.find(NewStaticInst);
    if (PlanIter == this->InstPlanMap.end()) {
      continue;
    }

    const auto &TransformPlan = PlanIter->second;
    if (TransformPlan.Plan == StreamTransformPlan::PlanT::DELETE) {
      /**
       * This is important to make the future instruction not dependent on
       * this deleted instruction.
       */
      this->Trace->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
          NewStaticInst, std::list<DynamicId>());

      auto NewDynamicId = NewDynamicInst->getId();
      this->Trace->commitDynamicInst(NewDynamicId);
      this->Trace->DynamicInstructionList.erase(NewInstIter);
      /**
       * No more handling for the deleted instruction.
       */
      continue;
    } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STEP) {

      this->Trace->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
          NewStaticInst, std::list<DynamicId>());

      auto NewDynamicId = NewDynamicInst->getId();
      this->Trace->commitDynamicInst(NewDynamicId);

      auto StepInst = new StreamStepInst();
      *NewInstIter = StepInst;
      this->Trace->AliveDynamicInstsMap.emplace(StepInst->getId(), NewInstIter);

    } else if (TransformPlan.Plan == StreamTransformPlan::PlanT::STORE) {

      assert(llvm::isa<llvm::StoreInst>(NewStaticInst) &&
             "STORE plan for non store instruction.");

      this->Trace->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
          NewStaticInst, std::list<DynamicId>());

      /**
       * REPLACE the original store instruction with our special stream store.
       */
      auto NewDynamicId = NewDynamicInst->getId();
      delete NewDynamicInst;

      auto StoreInst = new StreamStoreInst(NewDynamicId);
      *NewInstIter = StoreInst;
    }

    /**
     * Handle the use information.
     */
    for (auto &UsedStream : TransformPlan.getUsedStreams()) {
    }
  }

  DEBUG(llvm::errs() << "Stream: Transform done.\n");
}

} // namespace

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);