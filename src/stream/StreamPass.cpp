
#include "MemoryFootprint.h"
#include "MemoryPattern.h"
#include "Replay.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"

#include <sstream>

#define DEBUG_TYPE "StreamPass"

namespace {

class Stream {
public:
  Stream(const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
         size_t _LoopLevel, std::function<bool(llvm::PHINode *)> IsInductionVar)
      : Inst(_Inst), Loop(_Loop), LoopLevel(_LoopLevel), Qualified(false),
        TotalIters(0), TotalAccesses(0), TotalStreams(0), Iters(1),
        LastAccessIters(0), StartId(DynamicInstruction::InvalidId), Pattern() {
    assert(Utils::isMemAccessInst(this->Inst) &&
           "Should be load/store instruction to build a stream.");
    this->searchAddressComputeInstructions(IsInductionVar);
  }

  void addAccess(uint64_t Addr, DynamicInstruction::DynamicId DynamicId) {
    if (this->StartId == DynamicInstruction::InvalidId) {
      this->StartId = DynamicId;
    }
    this->Footprint.access(Addr);
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
    this->TotalIters++;
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

  void addDependentStream(Stream *DependentStream) {
    this->DependentStreams.insert(DependentStream);
  }
  void addBaseStream(Stream *BaseStream) {
    this->BaseStreams.insert(BaseStream);
  }
  void addBaseInductionVarStream(InductionVarStream *BaseInductionVarStream) {
    this->BaseInductionVarStreams.insert(BaseInductionVarStream);
  }

  void markQualified() { this->Qualified = true; }

  void finalizePattern() { this->Pattern.finalizePattern(); }

  const llvm::Loop *getLoop() const { return this->Loop; }
  const llvm::Instruction *getInst() const { return this->Inst; }
  size_t getLoopLevel() const { return this->LoopLevel; }
  const MemoryFootprint &getFootprint() const { return this->Footprint; }
  bool getQualified() const { return this->Qualified; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  DynamicInstruction::DynamicId getStartId() const { return this->StartId; }
  const MemoryPattern &getPattern() const { return this->Pattern; }

  bool isIndirect() const { return !this->BaseLoads.empty(); }
  size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
  size_t getNumAddrInsts() const { return this->AddrInsts.size(); }
  const std::unordered_set<llvm::Instruction *> &getAddrInsts() const {
    return this->AddrInsts;
  }
  const std::unordered_set<llvm::LoadInst *> &getBaseLoads() const {
    return this->BaseLoads;
  }
  const std::unordered_set<llvm::PHINode *> &getBaseInductionVars() const {
    return this->BaseInductionVars;
  }
  const std::unordered_set<Stream *> &getDependentStreams() const {
    return this->DependentStreams;
  }
  const std::unordered_set<Stream *> &getBaseStreams() const {
    return this->BaseStreams;
  }
  const std::unordered_set<InductionVarStream *> &
  getBaseInductionVarStreams() const {
    return this->BaseInductionVarStreams;
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
  MemoryFootprint Footprint;
  std::unordered_set<llvm::LoadInst *> BaseLoads;
  std::unordered_set<llvm::PHINode *> BaseInductionVars;
  std::unordered_set<llvm::Instruction *> AddrInsts;
  std::unordered_set<llvm::Instruction *> AliasInsts;

  std::unordered_set<Stream *> DependentStreams;
  std::unordered_set<Stream *> BaseStreams;
  std::unordered_set<InductionVarStream *> BaseInductionVarStreams;
  bool Qualified;

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
  MemoryPattern Pattern;

  void searchAddressComputeInstructions(
      std::function<bool(llvm::PHINode *)> IsInductionVar);
};

struct StreamTransformPlan {
public:
  enum PlanT {
    NOTHING,
    DELETE,
    STEP,
    STORE,
  } Plan;

  std::unordered_set<const llvm::Instruction *> UsedStreams;

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

  void addUsedStream(const llvm::Instruction *UsedStream) {
    this->UsedStreams.insert(UsedStream);
  }
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
  void initializeStreamIfNecessary(const LoopStackT &LoopStack,
                                   llvm::Instruction *Inst);

  /*************************************************************
   * Stream Choice.
   *************************************************************/
  void chooseStream();
  void buildDependenceGraph();
  void markQualifiedStream();
  Stream *getStreamByInstLoop(llvm::Instruction *Inst, const llvm::Loop *Loop);

  /*************************************************************
   * Stream transform.
   *************************************************************/
  void splitIVStreamFromDirectStream();
  void makeStreamTransformPlan();
  void makeIVStreamTransformPlan(InductionVarStream *IVStream);
  void transformStream();

  std::unordered_map<llvm::Instruction *, std::list<Stream>> InstStreamMap;

  std::unordered_map<const llvm::Loop *,
                     std::unordered_map<const llvm::Instruction *, Stream *>>
      ChosenLoopInstStreamMap;

  std::unordered_map<const llvm::PHINode *, InductionVarStream>
      PHINodeInductionVarStreamMap;

  std::unordered_map<const llvm::Instruction *, StreamTransformPlan>
      InstPlanMap;

  std::unordered_set<const llvm::Loop *> InitializedLoops;
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
    // auto BaseInductionVars = S.getBaseInductionVars();
    // if (BaseInductionVars.size() > 1) {
    //   return "MULTI_IV";
    // } else if (BaseInductionVars.size() == 1) {
    //   const auto &BaseIV = *BaseInductionVars.begin();
    //   auto BaseIVStreamIter =
    //   this->PHINodeInductionVarStreamMap.find(BaseIV); if (BaseIVStreamIter
    //   != this->PHINodeInductionVarStreamMap.end()) {
    //     const auto &BaseIVStream = BaseIVStreamIter->second;
    //     if (BaseIVStream.getPattern().computed()) {
    //       auto Pattern = BaseIVStream.getPattern().getPattern().AddrPattern;
    //       if (Pattern <= MemoryPattern::AddressPattern::QUARDRIC) {
    //         return "AFFINE_IV";
    //       }
    //     }
    //   }
    //   return "RANDOM_IV";
    // } else {
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
    // }
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
      size_t Footprint = Stream.getFootprint().getNumCacheLinesAccessed();
      size_t AddrInsts = Stream.getNumAddrInsts();

      std::string Qualified = "NO";
      if (Stream.getQualified()) {
        Qualified = "YES";
      }

      std::string Chosen = "NO";
      {
        auto ChosenLoopInstStreamMapIter =
            this->ChosenLoopInstStreamMap.find(Stream.getLoop());
        if (ChosenLoopInstStreamMapIter !=
            this->ChosenLoopInstStreamMap.end()) {
          if (ChosenLoopInstStreamMapIter->second.count(Stream.getInst()) !=
              0) {
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
    O << ' ' << "NO";                            // Qualified
    O << ' ' << "NO" << '\n';                    // Chosen
  }

  assert(TotalAccesses == this->DynMemInstCount && "Mismatch total accesses.");

  O << "--------------- Loop -----------------\n";
  O << "--------------- IV Stream ------------\n";
  for (auto &PHINodeIVStreamEntry : this->PHINodeInductionVarStreamMap) {
    auto &PHINode = PHINodeIVStreamEntry.first;
    auto &IVStream = PHINodeIVStreamEntry.second;
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

void StreamPass::transform() {
  this->analyzeStream();
  for (auto &InstStreamEntry : this->InstStreamMap) {
    for (auto &Stream : InstStreamEntry.second) {
      Stream.finalizePattern();
    }
  }
  this->chooseStream();
  // this->splitIVStreamFromDirectStream();

  // for (auto &PHINodeIVStreamEntry : this->PHINodeInductionVarStreamMap) {
  //   this->makeIVStreamTransformPlan(&(PHINodeIVStreamEntry.second));
  // }

  // this->makeStreamTransformPlan();

  // delete this->Trace;
  // this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  // this->transformStream();
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
  auto IsInductionVar = [this](llvm::PHINode *PHINode) -> bool {
    // return this->PHINodeInductionVarStreamMap.count(PHINode) != 0;
    return false;
  };
  while (LoopIter != LoopStack.rend()) {
    auto LoopLevel = Streams.size();
    Streams.emplace_back(Inst, *LoopIter, LoopLevel, IsInductionVar);
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

void StreamPass::activateIVStream(ActiveIVStreamMapT &ActiveIVStreams,
                                  llvm::PHINode *PHINode) {
  auto PHINodeInductionVarStreamMapIter =
      this->PHINodeInductionVarStreamMap.find(PHINode);
  if (PHINodeInductionVarStreamMapIter ==
      this->PHINodeInductionVarStreamMap.end()) {
    return;
  }

  auto *IVStream = &PHINodeInductionVarStreamMapIter->second;
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
    ActivePHINodeIVStreamMap.emplace(PHINode, IVStream);
  }
}

void StreamPass::pushLoopStack(LoopStackT &LoopStack,
                               ActiveStreamMapT &ActiveStreams,
                               ActiveIVStreamMapT &ActiveIVStreams,
                               llvm::Loop *Loop) {

  LoopStack.emplace_back(Loop);

  if (this->InitializedLoops.count(Loop) == 0) {
    // Find the memory access belongs to this level.
    std::unordered_set<llvm::Instruction *> MemAccessInsts;
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

        MemAccessInsts.insert(StaticInst);
      }
    }

    this->MemorizedMemoryAccess.emplace(
        std::piecewise_construct, std::forward_as_tuple(Loop),
        std::forward_as_tuple(std::move(MemAccessInsts)));

    // Build all the induction var streams.
    auto HeaderBB = Loop->getHeader();
    auto PHINodes = HeaderBB->phis();
    for (auto PHINodeIter = PHINodes.begin(), PHINodeEnd = PHINodes.end();
         PHINodeIter != PHINodeEnd; ++PHINodeIter) {
      auto PHINode = &*PHINodeIter;
      auto ComputeInsts = InductionVarStream::searchComputeInsts(PHINode, Loop);
      if (InductionVarStream::isInductionVarStream(PHINode, ComputeInsts)) {
        this->PHINodeInductionVarStreamMap.emplace(
            std::piecewise_construct, std::forward_as_tuple(PHINode),
            std::forward_as_tuple(PHINode, Loop, std::move(ComputeInsts)));
        DEBUG(llvm::errs()
              << "Initialized induction variable stream as "
              << this->PHINodeInductionVarStreamMap.at(PHINode).format()
              << '\n');
      }
    }

    this->InitializedLoops.insert(Loop);
  }

  /**
   * Activate the induction variable streams.
   */
  {
    auto HeaderBB = Loop->getHeader();
    auto PHINodes = HeaderBB->phis();
    for (auto PHINodeIter = PHINodes.begin(), PHINodeEnd = PHINodes.end();
         PHINodeIter != PHINodeEnd; ++PHINodeIter) {
      auto PHINode = &*PHINodeIter;
      this->activateIVStream(ActiveIVStreams, PHINode);
    }
  }

  /**
   * Initialize and activate the normal streams.
   */
  auto Iter = this->MemorizedMemoryAccess.find(Loop);
  assert(Iter != this->MemorizedMemoryAccess.end() &&
         "An initialized loop should have memorized memory accesses.");

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
                         ActiveStreamMapT &ActiveStreams,
                         ActiveIVStreamMapT &ActiveIVStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when endIter is called.");
  const auto &EndedLoop = LoopStack.back();
  {
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
  {
    auto ActiveIVStreamsIter = ActiveIVStreams.find(EndedLoop);
    if (ActiveIVStreamsIter != ActiveIVStreams.end()) {
      for (auto &PHINodeIVStreamEntry : ActiveIVStreamsIter->second) {
        auto &IVStream = PHINodeIVStreamEntry.second;
        assert(IVStream->getLoop() == EndedLoop &&
               "Mismatch on loop for streams in ActiveStreams.");
        IVStream->endIter();
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
      this->addAccess(LoopStack, ActiveStreams, *NewInstIter);
      this->handleAlias(*NewInstIter);
      this->CacheWarmerPtr->addAccess(Utils::getMemAddr(*NewInstIter));
    }

    /**
     * Handle the value for induction variable streams.
     */
    if (!LoopStack.empty()) {
      auto TopLoop = LoopStack.back();
      auto ActiveIVStreamsIter = ActiveIVStreams.find(TopLoop);
      if (ActiveIVStreamsIter != ActiveIVStreams.end()) {
        auto &ActivePHINodeIVStreamMap = ActiveIVStreamsIter->second;
        for (unsigned OperandIdx = 0,
                      NumOperands = NewStaticInst->getNumOperands();
             OperandIdx != NumOperands; ++OperandIdx) {
          auto OperandValue = NewStaticInst->getOperand(OperandIdx);
          if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {
            auto ActivePHINodeIVStreamMapIter =
                ActivePHINodeIVStreamMap.find(PHINode);
            if (ActivePHINodeIVStreamMapIter !=
                ActivePHINodeIVStreamMap.end()) {
              uint64_t Value = std::stoul(
                  NewDynamicInst->DynamicOperands[OperandIdx]->Value);
              ActivePHINodeIVStreamMapIter->second->addAccess(Value);
            }
          }
        }
      }
    }
  }
}

void Stream::searchAddressComputeInstructions(
    std::function<bool(llvm::PHINode *)> IsInductionVar) {

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

    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(CurrentInst)) {
      if (IsInductionVar(PHINode)) {
        this->BaseInductionVars.insert(PHINode);
        // Do not go further for induction variables.
        continue;
      }
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

Stream *StreamPass::getStreamByInstLoop(llvm::Instruction *Inst,
                                        const llvm::Loop *Loop) {
  auto Iter = this->InstStreamMap.find(Inst);
  if (Iter == this->InstStreamMap.end()) {
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

void StreamPass::buildDependenceGraph() {
  for (auto &InstStreamEntry : this->InstStreamMap) {
    auto &Streams = InstStreamEntry.second;
    for (auto &S : Streams) {
      if (!S.isIndirect()) {
        continue;
      }
      auto Loop = S.getLoop();
      for (const auto &BaseLoad : S.getBaseLoads()) {
        auto BaseStream = this->getStreamByInstLoop(BaseLoad, Loop);
        if (BaseStream != nullptr) {
          BaseStream->addDependentStream(&S);
          S.addBaseStream(BaseStream);
        }
      }
    }
  }
}

void StreamPass::markQualifiedStream() {
  /**
   * Use direct affine stream as the start point to propagate qualified flag
   * in the dependence graph.
   */
  std::list<Stream *> Queue;
  for (auto &InstStreamEntry : this->InstStreamMap) {
    auto &Streams = InstStreamEntry.second;
    for (auto &S : Streams) {
      if (S.isIndirect() || S.isAliased()) {
        continue;
      }
      // Direct stream without alias.
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
    if (S->getQualified()) {
      // We have already processed this stream.
      continue;
    }
    S->markQualified();
    // Check all the dependent streams
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (S->isAliased()) {
        continue;
      }
      bool Qualified = true;
      for (const auto &BaseStream : DependentStream->getBaseStreams()) {
        if (!BaseStream->getQualified()) {
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

void StreamPass::chooseStream() {
  this->buildDependenceGraph();
  this->markQualifiedStream();

  for (auto &InstStreamEntry : this->InstStreamMap) {
    auto &Streams = InstStreamEntry.second;
    // Try to pick the highest level of qualified stream.
    Stream *ChosenStream = nullptr;
    for (auto &S : Streams) {
      if (S.getQualified()) {
        ChosenStream = &S;
      }
    }
    if (ChosenStream != nullptr) {
      auto Loop = ChosenStream->getLoop();
      auto Inst = ChosenStream->getInst();
      auto ChosenLoopInstStreamMapIter =
          this->ChosenLoopInstStreamMap.find(Loop);
      if (ChosenLoopInstStreamMapIter == this->ChosenLoopInstStreamMap.end()) {
        ChosenLoopInstStreamMapIter =
            this->ChosenLoopInstStreamMap
                .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                         std::forward_as_tuple())
                .first;
      }
      assert(ChosenLoopInstStreamMapIter->second.count(Inst) == 0 &&
             "This stream is already chosen.");
      ChosenLoopInstStreamMapIter->second.emplace(Inst, ChosenStream);
    }
  }
}

void StreamPass::splitIVStreamFromDirectStream() {
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStreamMap) {
    const auto &Loop = LoopInstStreamEntry.first;
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      const auto &Inst = InstStreamEntry.first;
      Stream *S = InstStreamEntry.second;
      if (S->isIndirect()) {
        continue;
      }
      /**
       * Check if we need to split the induction var stream out
       * by looking at how the address instructions are used outside the
       * stream.
       */
      const auto &AddrInsts = S->getAddrInsts();
      bool OutsideUser = false;
      for (auto &AddrInst : AddrInsts) {
        if (OutsideUser) {
          break;
        }
        for (auto U : AddrInst->users()) {
          if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
            // An instruction uses AddrInsts.
            if (I == Inst) {
              // Used by the stream itself, ignore this case.
              continue;
            }
            if (AddrInsts.count(I) != 0) {
              OutsideUser = true;
              break;
            }
          }
        }
      }
      if (!OutsideUser) {
        // Rare case: no outside user, we do not have to split this stream.
        continue;
      }
      // We split out the induction var stream of all the phi nodes.
      for (auto &AddrInst : AddrInsts) {
        if (AddrInst->getParent() != Loop->getHeader()) {
          continue;
        }
        if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(AddrInst)) {
          DEBUG(llvm::errs()
                << "Split induction variable stream "
                << LoopUtils::formatLLVMInst(PHINode) << " from direct stream "
                << LoopUtils::formatLLVMInst(Inst) << '\n');
          auto Iter = this->PHINodeInductionVarStreamMap.find(PHINode);
          if (Iter == this->PHINodeInductionVarStreamMap.end()) {
            auto ComputeInsts =
                InductionVarStream::searchComputeInsts(PHINode, Loop);
            Iter = this->PHINodeInductionVarStreamMap
                       .emplace(std::piecewise_construct,
                                std::forward_as_tuple(PHINode),
                                std::forward_as_tuple(PHINode, Loop,
                                                      std::move(ComputeInsts)))
                       .first;
            DEBUG(llvm::errs() << Iter->second.format() << '\n');
          }
          S->addBaseInductionVarStream(&(Iter->second));
        }
      }
    }
  }
}

void StreamPass::makeIVStreamTransformPlan(InductionVarStream *IVStream) {
  DEBUG(llvm::errs() << "Make IVStream tranform plan at level "
                     << LoopUtils::getLoopId(IVStream->getLoop())
                     << " for IVStream "
                     << LoopUtils::formatLLVMInst(IVStream->getPHIInst())
                     << '\n');
  for (const auto &ComputeInst : IVStream->getComputeInsts()) {
    auto PlanIter = this->InstPlanMap.find(ComputeInst);
    if (PlanIter == this->InstPlanMap.end()) {
      PlanIter = this->InstPlanMap
                     .emplace(std::piecewise_construct,
                              std::forward_as_tuple(ComputeInst),
                              std::forward_as_tuple())
                     .first;
    }
    if (InductionVarStream::isStepInst(ComputeInst)) {
      PlanIter->second.Plan = StreamTransformPlan::PlanT::STEP;
    } else {
      switch (PlanIter->second.Plan) {
      case StreamTransformPlan::PlanT::NOTHING:
      case StreamTransformPlan::PlanT::DELETE: {
        PlanIter->second.Plan = StreamTransformPlan::PlanT::DELETE;
        break;
      }
      }
    }
    DEBUG(
        llvm::errs() << "Select transform plan for compute inst "
                     << LoopUtils::formatLLVMInst(ComputeInst) << " to "
                     << StreamTransformPlan::formatPlanT(PlanIter->second.Plan)
                     << '\n');

    // Add uses for all the users.
    for (auto U : ComputeInst->users()) {
      if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
        auto UserPlanIter = this->InstPlanMap.find(I);
        if (UserPlanIter == this->InstPlanMap.end()) {
          UserPlanIter =
              this->InstPlanMap
                  .emplace(std::piecewise_construct, std::forward_as_tuple(I),
                           std::forward_as_tuple())
                  .first;
        }
        UserPlanIter->second.addUsedStream(IVStream->getPHIInst());
        DEBUG(llvm::errs() << "Add used stream for user "
                           << LoopUtils::formatLLVMInst(I) << " with IV stream "
                           << LoopUtils::formatLLVMInst(IVStream->getPHIInst())
                           << '\n');
      }
    }
  }
}

void StreamPass::makeStreamTransformPlan() {
  for (auto &LoopInstStreamEntry : this->ChosenLoopInstStreamMap) {
    for (auto &InstStreamEntry : LoopInstStreamEntry.second) {
      Stream *S = InstStreamEntry.second;

      DEBUG(llvm::errs() << "make stream transform plan at level "
                         << LoopUtils::getLoopId(S->getLoop()) << " for stream "
                         << LoopUtils::formatLLVMInst(S->getInst()) << '\n');

      for (const auto &AddrInst : S->getAddrInsts()) {
        // Simply mark them as delete.
        auto PlanIter = this->InstPlanMap.find(AddrInst);
        if (PlanIter == this->InstPlanMap.end()) {
          PlanIter = this->InstPlanMap
                         .emplace(std::piecewise_construct,
                                  std::forward_as_tuple(AddrInst),
                                  std::forward_as_tuple())
                         .first;
        }
        switch (PlanIter->second.Plan) {
        case StreamTransformPlan::PlanT::NOTHING:
        case StreamTransformPlan::PlanT::DELETE: {
          PlanIter->second.Plan = StreamTransformPlan::PlanT::DELETE;
          break;
        }
        }
        DEBUG(llvm::errs() << "Select transform plan for addr inst "
                           << LoopUtils::formatLLVMInst(AddrInst) << " to "
                           << StreamTransformPlan::formatPlanT(
                                  PlanIter->second.Plan)
                           << '\n');
      }

      // Add uses for all the users.
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
          UserPlanIter->second.addUsedStream(Inst);
          DEBUG(llvm::errs()
                << "Add used stream for user " << LoopUtils::formatLLVMInst(I)
                << " with stream " << LoopUtils::formatLLVMInst(Inst) << '\n');
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
        InstPlanIter->second.Plan = StreamTransformPlan::PlanT::STORE;
      } else {
        // This is a load.
        InstPlanIter->second.Plan = StreamTransformPlan::PlanT::DELETE;
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

  // A map from stream to the previous instruction (config/step).
  // Used to resovle dependence to the FIFO.
  std::unordered_map<llvm::Instruction *, DynamicInstruction::DynamicId>
      StreamPrevInstMap;

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
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      break;
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
  }

  DEBUG(llvm::errs() << "Stream: Transform done.\n");
}

} // namespace

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);