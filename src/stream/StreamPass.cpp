#include "DynamicLoopTree.h"
#include "MemoryAccessPattern.h"
#include "Replay.h"
#include "trace/ProfileParser.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#define DEBUG_TYPE "StreamPass"

namespace {

class ScalarUIntVar {
public:
  ScalarUIntVar(const std::string &_name) : Val(0), name(_name) {}
  uint64_t Val;
  std::string name;
  void print(llvm::raw_ostream &O) const {
    O << this->name << ": " << this->Val << '\n';
  }
  void print(std::ostream &O) const {
    O << this->name << ": " << this->Val << '\n';
  }
};

class StreamLoadInst : public DynamicInstruction {
public:
  StreamLoadInst(DynamicId Id) : DynamicInstruction() {
    // Hack: override my new id with the id of the original load inst.
    this->Id = Id;
  }
  std::string getOpName() const override { return "stream-load"; }
};

class StreamStoreInst : public DynamicInstruction {
public:
  StreamStoreInst(DynamicId Id) : DynamicInstruction() {
    // Same hack.
    this->Id = Id;
  }
  std::string getOpName() const override { return "stream-store"; }
};

/**
 * Stream class wraps over MemoryAccessPattern and contains the stream pattern
 * analyzed in a specific loop level.
 */
class Stream {
public:
  Stream(llvm::Loop *_Loop, llvm::Instruction *_MemInst)
      : Loop(_Loop), MemInst(_MemInst), AddrValue(nullptr), Pattern(_MemInst) {
    assert(this->Loop->contains(this->MemInst) &&
           "The loop should contain the memory access instruction.");

    if (llvm::isa<llvm::LoadInst>(this->MemInst)) {
      this->AddrValue = this->MemInst->getOperand(0);
    } else {
      this->AddrValue = this->MemInst->getOperand(1);
    }

    // Intialize the address computing instructions.
    this->calculateAddressComputeInsts();
  }

  void addAccess(uint64_t Addr) { this->Pattern.addAccess(Addr); }
  void addMissingAccess() { this->Pattern.addMissingAccess(); }
  void endStream() { this->Pattern.endStream(); }
  void finalizePattern() { this->Pattern.finalizePattern(); }

  llvm::Loop *getLoop() const { return this->Loop; }
  llvm::Instruction *getMemInst() const { return this->MemInst; }
  llvm::Value *getAddrValue() const { return this->AddrValue; }
  const MemoryAccessPattern &getPattern() const { return this->Pattern; }
  const std::set<llvm::Instruction *> &getAddressComputeInsts() const {
    return this->AddressComputeInsts;
  }

private:
  llvm::Loop *Loop;
  llvm::Instruction *MemInst;
  llvm::Value *AddrValue;
  MemoryAccessPattern Pattern;
  std::set<llvm::Instruction *> AddressComputeInsts;

  /**
   * Calculate the address compute instructions.
   * This is done via a DFS from the address value and try to include every GEP,
   * PHINode, CAST and ARITHMETIC instructions within the same level of loop.
   */
  void calculateAddressComputeInsts();
};

class StreamPass : public ReplayTrace {
public:
  static char ID;

#define INIT_VAR(var) var(#var)
  StreamPass()
      : ReplayTrace(ID), INIT_VAR(DynInstCount), INIT_VAR(DynMemInstCount),
        INIT_VAR(TracedDynInstCount), INIT_VAR(AddRecLoadCount),
        INIT_VAR(AddRecStoreCount), INIT_VAR(ConstantCount),
        INIT_VAR(LinearCount), INIT_VAR(QuardricCount), INIT_VAR(IndirectCount),
        INIT_VAR(StreamCount), INIT_VAR(RemovedAddrInstCount) {}
#undef INIT_VAR

protected:
  bool initialize(llvm::Module &Module) override {
    bool Ret = ReplayTrace::initialize(Module);

    this->State = SEARCHING;
    this->LoopTree = nullptr;

    this->LoopTripCount.clear();
    this->MemorizedMemoryAccess.clear();

    return Ret;
  }

  bool finalize(llvm::Module &Module) override {

    this->State = SEARCHING;
    if (this->LoopTree != nullptr) {
      delete this->LoopTree;
      this->LoopTree = nullptr;
    }

    this->LoopTripCount.clear();
    return ReplayTrace::finalize(Module);
  }

  void dumpStats(std::ostream &O) override;

  void transform() override;

  /**
   * Analyze the stream pattern in the first pass.
   */
  void analyzeStream();

  /**
   * Replace the memory access with our stream interface in the second pass.
   */
  void transformStream();

  bool isLoopStream(llvm::Loop *Loop);

  /**
   * Initialize the MemInstToStreamMap and MemorizedMemoryAccess.
   */
  void initializeLoopStream(llvm::Loop *Loop);

  bool isStreamAccess(llvm::ScalarEvolution *SE,
                      llvm::Instruction *StaticInst) const;

  /**
   * For all the analyzed stream, choose the loop level for optimization.
   */
  void chooseStream();

  void updateTripCount(DynamicLoopIteration *LoopTree);

  void computeMemoryAccessPattern(DynamicLoopIteration *LoopTree);

  void computeStreamStatistics();

  enum {
    SEARCHING,
    STREAMING,
  } State;

  ProfileParser Profile;

  /**
   * A map from memory access instruction to a list of streams.
   * Each stream contains the analyzation result of the instruction in different
   * loop levels.
   */
  std::unordered_map<llvm::Instruction *, std::list<Stream>> MemInstToStreamMap;

  /**
   * A map from memory access instruction to the chosen stream at a specific
   * loop level. The stream is actually stored in MemInstToStreamMap. Be careful
   * with the life span.
   */
  std::unordered_map<llvm::Instruction *, Stream *> MemInstToChosenStreamMap;

  /**
   * A set contains the removed address compute intsruction.
   */
  std::unordered_set<llvm::Instruction *> RemovableAddrComputeInstSet;

  DynamicLoopIteration *LoopTree;

  std::unordered_map<llvm::Loop *, bool> MemorizedLoopStream;
  std::unordered_map<llvm::Loop *, std::unordered_set<llvm::Instruction *>>
      MemorizedMemoryAccess;

  std::unordered_map<llvm::Loop *, size_t> LoopOngoingIters;

  std::unordered_map<llvm::Loop *, std::pair<uint64_t, uint64_t>> LoopTripCount;

  /*******************************************************
   * Statistics.
   *******************************************************/
  ScalarUIntVar DynInstCount;
  ScalarUIntVar DynMemInstCount;
  ScalarUIntVar TracedDynInstCount;

  ScalarUIntVar AddRecLoadCount;
  ScalarUIntVar AddRecStoreCount;
  ScalarUIntVar ConstantCount;
  ScalarUIntVar LinearCount;
  ScalarUIntVar QuardricCount;
  ScalarUIntVar IndirectCount;
  ScalarUIntVar StreamCount;
  ScalarUIntVar RemovedAddrInstCount;
}; // namespace

void StreamPass::dumpStats(std::ostream &O) {
  O << "--------------- Stream -----------------\n";
  this->DynInstCount.print(O);
  this->DynMemInstCount.print(O);
  this->TracedDynInstCount.print(O);
  this->AddRecLoadCount.print(O);
  this->AddRecStoreCount.print(O);
  this->ConstantCount.print(O);
  this->LinearCount.print(O);
  this->QuardricCount.print(O);
  this->IndirectCount.print(O);
  this->StreamCount.print(O);
  this->RemovedAddrInstCount.print(O);

  for (auto &MemInstStreamEntry : this->MemInstToStreamMap) {
    for (auto &Stream : MemInstStreamEntry.second) {
      const auto &Pattern = Stream.getPattern().getPattern();
      O << LoopUtils::getLoopId(Stream.getLoop()) << ' ';
      O << LoopUtils::formatLLVMInst(Stream.getMemInst()) << ' '
        << MemoryAccessPattern::formatPattern(Pattern.CurrentPattern) << ' '
        << MemoryAccessPattern::formatAccessPattern(Pattern.AccPattern) << ' '
        << Pattern.Iters << ' ' << Pattern.Accesses << ' ' << Pattern.Updates
        << ' ' << Pattern.StreamCount << ' ';
      if (Pattern.BaseLoad != nullptr) {
        O << LoopUtils::formatLLVMInst(Pattern.BaseLoad);
      } else {
        O << "NO_BASE_LOAD";
      }
      O << '\n';
    }
  }
}

void StreamPass::transform() {
  // First pass.
  this->analyzeStream();

  for (auto &MemInstStreamEntry : this->MemInstToStreamMap) {
    for (auto &Stream : MemInstStreamEntry.second) {
      if (!Stream.getPattern().computed()) {
        llvm::errs() << LoopUtils::getLoopId(Stream.getLoop()) << " "
                     << LoopUtils::formatLLVMInst(Stream.getMemInst())
                     << " is not computed.\n";
      } else {
        DEBUG(llvm::errs() << LoopUtils::getLoopId(Stream.getLoop()) << " "
                           << LoopUtils::formatLLVMInst(Stream.getMemInst())
                           << " is computed.\n");
      }
      Stream.finalizePattern();
    }
  }

  this->chooseStream();

  delete this->Trace;
  this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  // Second pass.
  this->transformStream();

  this->computeStreamStatistics();
}

void StreamPass::chooseStream() {
  // First pass to pick out linear and quardric streams.
  for (auto &MemInstToStreamEntry : this->MemInstToStreamMap) {
    Stream *ChosenStream = nullptr;
    for (auto &Stream : MemInstToStreamEntry.second) {
      switch (Stream.getPattern().getPattern().CurrentPattern) {
      case MemoryAccessPattern::Pattern::QUARDRIC:
      case MemoryAccessPattern::Pattern::LINEAR: {
        ChosenStream = &Stream;
      }
      default: { break; }
      }
    }
    if (ChosenStream != nullptr) {
      this->MemInstToChosenStreamMap.emplace(MemInstToStreamEntry.first,
                                             ChosenStream);
    }
  }
  // Second pass to make the indirect stream same loop level as the base load.
  for (auto &MemInstToStreamEntry : this->MemInstToStreamMap) {
    Stream *ChosenStream = nullptr;
    for (auto &Stream : MemInstToStreamEntry.second) {
      const auto &Pattern = Stream.getPattern().getPattern();
      if (Pattern.CurrentPattern == MemoryAccessPattern::Pattern::INDIRECT) {
        auto Iter = this->MemInstToChosenStreamMap.find(Pattern.BaseLoad);
        if (Iter != this->MemInstToChosenStreamMap.end()) {
          if (Iter->second->getLoop() == Stream.getLoop()) {
            // We found the match.
            // No need to search for next one.
            ChosenStream = &Stream;
            break;
          }
        }
      }
    }
    if (ChosenStream != nullptr) {
      this->MemInstToChosenStreamMap.emplace(MemInstToStreamEntry.first,
                                             ChosenStream);
    }
  }
  // Finally try to find the removable address compute instruction.
  // For now I take a very conservative approach: only look at the address value
  // and see if that is within the chosen loop level and also has only one user.
  for (const auto &MemInstToChosenStreamEntry :
       this->MemInstToChosenStreamMap) {
    const auto &ChosenStream = MemInstToChosenStreamEntry.second;
    if (auto AddrInst =
            llvm::dyn_cast<llvm::Instruction>(ChosenStream->getAddrValue())) {
      if (!ChosenStream->getLoop()->contains(AddrInst)) {
        continue;
      }
      if (!AddrInst->hasOneUse()) {
        continue;
      }
      // This is a removable address compute instruction.
      this->RemovableAddrComputeInstSet.insert(AddrInst);
    }
  }
}

void StreamPass::analyzeStream() {
  llvm::Loop *CurrentLoop = nullptr;

  DEBUG(llvm::errs() << "Stream: Start analysis.\n");

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;

    bool IsAtHeadOfCandidate = false;

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewInstIter)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

      if (llvm::isa<llvm::LoadInst>(NewStaticInst) ||
          llvm::isa<llvm::StoreInst>(NewStaticInst)) {
        this->DynMemInstCount.Val++;
      }

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        if (this->isLoopStream(NewLoop)) {
          IsAtHeadOfCandidate =
              LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
        } else {
          NewLoop = nullptr;
        }
      }
    } else {
      // We at the end of the loop. Commit everything.
      if (this->LoopTree != nullptr) {
        this->LoopTree->addInstEnd(this->Trace->DynamicInstructionList.end());

        this->updateTripCount(this->LoopTree);
        this->computeMemoryAccessPattern(this->LoopTree);

        delete this->LoopTree;
        this->LoopTree = nullptr;
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Trace->commitOneDynamicInst();
      }
      break;
    }

    switch (this->State) {
    case SEARCHING: {
      assert(this->Trace->DynamicInstructionList.size() <= 2 &&
             "For searching state, there should be at most 2 dynamic "
             "instructions in the buffer.");

      if (this->Trace->DynamicInstructionList.size() == 2) {
        // We can commit the previous one.
        this->Trace->commitOneDynamicInst();
      }

      if (IsAtHeadOfCandidate) {
        // Time the start STREAMING.

        // DEBUG(llvm::errs() << "Stream: Switch to STREAMING state.\n");

        this->State = STREAMING;
        CurrentLoop = NewLoop;
        this->LoopTree = new DynamicLoopIteration(
            NewLoop, this->Trace->DynamicInstructionList.end());
        // Initialize the loop stream.
        this->initializeLoopStream(CurrentLoop);
        // Add the first instruction to the loop tree.
        this->LoopTree->addInst(NewInstIter);

        // DEBUG(llvm::errs() << "Stream: Switch to STREAMING state: Done.\n");
      }

      break;
    }
    case STREAMING: {

      // Add the newest instruction into the tree.
      //   DEBUG(llvm::errs() << "Stream: Add instruction to loop tree.\n");
      this->LoopTree->addInst(NewInstIter);
      //   DEBUG(llvm::errs() << "Stream: Add instruction to loop tree:
      //   Done.\n");

      // Check if we have reached the number of iterations.
      if (this->LoopTree->countIter() == 4 || !CurrentLoop->contains(NewLoop)) {

        // Collect statistics in first pass.
        this->updateTripCount(this->LoopTree);
        this->computeMemoryAccessPattern(this->LoopTree);

        auto NextIter = this->LoopTree->moveTailIter();

        // Commit all the instructions.
        while (this->Trace->DynamicInstructionList.size() > 1) {
          this->Trace->commitOneDynamicInst();
        }

        // Clear our iters.
        delete this->LoopTree;
        this->LoopTree = NextIter;
        if (this->LoopTree == nullptr) {
          // We are not in the same loop.
          //   DEBUG(llvm::errs() << "Stream: Out of the current STREAMING
          //   loop.\n");
          if (IsAtHeadOfCandidate) {
            // We are in other candidate.
            CurrentLoop = NewLoop;
            this->LoopTree = new DynamicLoopIteration(
                NewLoop, this->Trace->DynamicInstructionList.end());
            this->LoopTree->addInst(NewInstIter);
          } else {
            // We are not in any candidate.
            // DEBUG(llvm::errs() << "Stream: Switch to SEARCHING state.\n");
            this->State = SEARCHING;
          }
        } else {
          // We are still in the same loop. Do nothing.
        }
      }

      break;
    }
    default: {
      llvm_unreachable("Stream: Invalid machine state.");
      break;
    }
    }
  }

  DEBUG(llvm::errs() << "Stream: End.\n");
}

bool StreamPass::isLoopStream(llvm::Loop *Loop) {

  assert(Loop != nullptr && "Loop should not be nullptr.");

  auto Iter = this->MemorizedLoopStream.find(Loop);
  if (Iter != this->MemorizedLoopStream.end()) {
    return Iter->second;
  }

  // We allow nested loops.
  bool IsLoopStream = true;
  if (!LoopUtils::isLoopContinuous(Loop)) {
    // DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
    //                    << " is not stream as it is not continuous.\n");

    IsLoopStream = false;
  }

  // Done: this loop can be represented as stream.
  // DEBUG(llvm::errs() << "isLoopStream returned true.\n");
  this->MemorizedLoopStream.emplace(Loop, IsLoopStream);
  return IsLoopStream;
}

void StreamPass::initializeLoopStream(llvm::Loop *Loop) {
  if (this->MemorizedMemoryAccess.find(Loop) !=
      this->MemorizedMemoryAccess.end()) {
    // We have alread initialized this loop stream.
    return;
  }

  // First initialize MemorizedMemoryAccess for every subloop.
  std::list<llvm::Loop *> Loops;
  Loops.push_back(Loop);
  while (!Loops.empty()) {
    auto CurrentLoop = Loops.front();
    Loops.pop_front();
    assert(this->MemorizedMemoryAccess.find(CurrentLoop) ==
               this->MemorizedMemoryAccess.end() &&
           "This loop stream is already initialized.");
    this->MemorizedMemoryAccess.emplace(std::piecewise_construct,
                                        std::forward_as_tuple(CurrentLoop),
                                        std::forward_as_tuple());
    for (auto SubLoop : CurrentLoop->getSubLoops()) {
      Loops.push_back(SubLoop);
    }
  }

  auto LI = this->CachedLI->getLoopInfo(Loop->getHeader()->getParent());
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;

    auto InnerMostLoop = LI->getLoopFor(BB);

    auto &MemAccess = this->MemorizedMemoryAccess.at(InnerMostLoop);

    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      if ((!llvm::isa<llvm::LoadInst>(Inst)) &&
          (!llvm::isa<llvm::StoreInst>(Inst))) {
        continue;
      }
      // Insert the memory access instruction into the memorized loop.
      MemAccess.insert(Inst);

      assert(this->MemInstToStreamMap.find(Inst) ==
                 this->MemInstToStreamMap.end() &&
             "The memory access instruction is already processed.");
      auto &StreamList =
          this->MemInstToStreamMap.emplace(Inst, std::list<Stream>())
              .first->second;

      // Initialize the stream for all loop level.
      auto LoopLevel = InnerMostLoop;
      while (LoopLevel != Loop) {
        StreamList.emplace_back(LoopLevel, Inst);
        LoopLevel = LoopLevel->getParentLoop();
      }
      StreamList.emplace_back(LoopLevel, Inst);
    }
  }
}

void StreamPass::transformStream() {

  // Grows at the end.
  std::list<llvm::Loop *> RegionStack;

  DEBUG(llvm::errs() << "Stream: Start Pass 2.\n");

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    if (NewInstIter == this->Trace->DynamicInstructionList.end()) {
      // We at the end of the loop. Commit everything.
      while (!this->Trace->DynamicInstructionList.empty()) {
        // Serialize in the second pass.
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      break;
    }

    // This is a new instruction.
    auto NewDynamicInst = *NewInstIter;
    auto NewStaticInst = NewDynamicInst->getStaticInstruction();
    assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

    auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
    auto NewLoop = LI->getLoopFor(NewStaticInst->getParent());

    bool IsAtHeadOfCandidate = false;

    if (NewLoop != nullptr) {
      if (this->isLoopStream(NewLoop)) {
        IsAtHeadOfCandidate =
            LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
      } else {
        NewLoop = nullptr;
      }
    }

    // Pop all the exited region.
    while (!RegionStack.empty() &&
           !RegionStack.back()->contains(NewStaticInst)) {
      RegionStack.pop_back();
    }

    // Enter new region.
    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      RegionStack.push_back(NewLoop);
    }

    // Do the transformation.
    // Replace the stream load/store instruction.
    // Remove the removable address compute instruction.
    if (this->RemovableAddrComputeInstSet.count(NewStaticInst) != 0) {

      // In order to maintain some sort of dependence chain, I will update the
      // register dependence look up table here.
      // This works as this is the last instruction in the stream so far, so the
      // translateRegisterDependence will automatically pick up the dependence.
      std::list<DynamicInstruction::DynamicId> RegDeps;
      for (const auto &RegDep :
           this->Trace->RegDeps.at(NewDynamicInst->getId())) {
        RegDeps.push_back(RegDep.second);
      }
      this->Trace->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
          NewStaticInst, std::move(RegDeps));

      // Remove from the dependence map and so on.
      this->Trace->commitDynamicInst(NewDynamicInst->getId());

      // Remove from the list.
      this->Trace->DynamicInstructionList.erase(NewInstIter);

      this->RemovedAddrInstCount.Val++;

    } else if (this->MemInstToChosenStreamMap.count(NewStaticInst) != 0) {
      bool IsLoad = llvm::isa<llvm::LoadInst>(NewStaticInst);
      DynamicInstruction *NewStreamInst = nullptr;
      if (IsLoad) {
        NewStreamInst = new StreamLoadInst(NewDynamicInst->getId());
      } else {
        NewStreamInst = new StreamStoreInst(NewDynamicInst->getId());
      }

      // Replace the original memory access with stream access.
      *NewInstIter = NewStreamInst;

      // Remember to delete the original one.
      delete NewDynamicInst;
    }

    // Serialize the instruction.
    // Set a sufficient large buffer size so that the conditional branch
    // instruction can find the next basic block.
    if (this->Trace->DynamicInstructionList.size() == 100) {
      this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                  this->Trace);
      this->Trace->commitOneDynamicInst();
    }
  }

  DEBUG(llvm::errs() << "Stream: End.\n");
}

void StreamPass::updateTripCount(DynamicLoopIteration *LoopTree) {
  assert(LoopTree != nullptr &&
         "Null LoopTree when trying to update trip count.");
  auto Loop = LoopTree->getLoop();
  auto TripCount = LoopTree->countIter();

  if (TripCount > 0) {
    // Update the trip count.
    if (this->LoopTripCount.find(Loop) == this->LoopTripCount.end()) {
      this->LoopTripCount.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(Loop),
                                  std::forward_as_tuple(TripCount, 1));
    } else {
      auto &Entry = this->LoopTripCount.at(Loop);
      Entry.first += TripCount;
      Entry.second++;
    }
  }

  // Count for all the nested loop recursively.
  auto Iter = LoopTree;
  for (size_t Trip = 0; Trip < TripCount; ++Trip) {
    for (auto NestIter = Loop->begin(), NestEnd = Loop->end();
         NestIter != NestEnd; ++NestIter) {
      auto ChildLoop = *NestIter;
      this->updateTripCount(Iter->getChildIter(ChildLoop));
    }
    Iter = Iter->getNextIter();
  }
}

void StreamPass::computeMemoryAccessPattern(DynamicLoopIteration *LoopTree) {
  assert(LoopTree != nullptr &&
         "Null LoopTree when trying to compute memory access pattern.");

  auto Loop = LoopTree->getLoop();
  auto TripCount = LoopTree->countIter();

  const auto &MemAccess = this->MemorizedMemoryAccess.at(Loop);

  auto Iter = LoopTree;
  for (size_t Trip = 0; Trip < TripCount; ++Trip) {

    // Count for all the nested loop recursively.
    for (auto NestIter = Loop->begin(), NestEnd = Loop->end();
         NestIter != NestEnd; ++NestIter) {
      auto ChildLoop = *NestIter;
      this->computeMemoryAccessPattern(Iter->getChildIter(ChildLoop));
    }

    // Compute pattern for memory access within this loop.
    for (auto MemAccessInst : MemAccess) {
      auto DynamicInst = Iter->getDynamicInst(MemAccessInst);
      auto &StreamList = this->MemInstToStreamMap.at(MemAccessInst);
      if (DynamicInst == nullptr) {
        // This memory access is not performed in this iteration.
        for (auto &Stream : StreamList) {
          Stream.addMissingAccess();
        }
      } else {
        uint64_t Addr = 0;
        if (llvm::isa<llvm::LoadInst>(MemAccessInst)) {
          Addr = DynamicInst->DynamicOperands[0]->getAddr();
        } else {
          Addr = DynamicInst->DynamicOperands[1]->getAddr();
        }
        for (auto &Stream : StreamList) {
          Stream.addAccess(Addr);
        }
      }
    }

    Iter = Iter->getNextIter();
  }

  if (Iter == nullptr) {
    // This is the end of the loop, we should end the stream.
    size_t PreviousIters = 0;
    if (this->LoopOngoingIters.find(Loop) != this->LoopOngoingIters.end()) {
      PreviousIters = this->LoopOngoingIters.at(Loop);
    }
    this->LoopOngoingIters.erase(Loop);
    // We have to end the stream for all memory instruction with in the loop.
    // TODO: Memorize this?
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        if ((!llvm::isa<llvm::LoadInst>(Inst)) &&
            (!llvm::isa<llvm::StoreInst>(Inst))) {
          continue;
        }
        auto &StreamList = this->MemInstToStreamMap.at(Inst);
        for (auto &Stream : StreamList) {
          if (Stream.getLoop() == Loop) {
            Stream.endStream();
            break;
          }
        }
      }
    }
  } else {
    // This loop is still going on.
    size_t PreviousIters = 0;
    if (this->LoopOngoingIters.find(Loop) != this->LoopOngoingIters.end()) {
      PreviousIters = this->LoopOngoingIters.at(Loop);
    }
    this->LoopOngoingIters[Loop] = PreviousIters + TripCount;
  }
}

bool StreamPass::isStreamAccess(llvm::ScalarEvolution *SE,
                                llvm::Instruction *StaticInst) const {
  if (!llvm::isa<llvm::LoadInst>(StaticInst) &&
      !llvm::isa<llvm::StoreInst>(StaticInst)) {
    return false;
  }

  bool IsLoad = llvm::isa<llvm::LoadInst>(StaticInst);
  llvm::Value *Addr = nullptr;
  if (IsLoad) {
    Addr = StaticInst->getOperand(0);
  } else {
    Addr = StaticInst->getOperand(1);
  }

  const llvm::SCEV *SCEV = SE->getSCEV(Addr);

  if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
    return true;
  }

  // Try to look at dynamic information.
  auto Iter = this->MemInstToStreamMap.find(StaticInst);
  if (Iter == this->MemInstToStreamMap.end()) {
    return false;
  }
  // We only look at the inner most loop level for now.
  assert(!Iter->second.empty() && "There should be at least one stream.");
  const auto &Pattern = Iter->second.front().getPattern().getPattern();
  switch (Pattern.CurrentPattern) {
  case MemoryAccessPattern::Pattern::CONSTANT:
  case MemoryAccessPattern::Pattern::QUARDRIC:
  case MemoryAccessPattern::Pattern::LINEAR: {
    return true;
  }
  case MemoryAccessPattern::Pattern::INDIRECT: {
    auto BaseLoad = Pattern.BaseLoad;

    auto BaseIter = this->MemInstToStreamMap.find(BaseLoad);
    if (BaseIter == this->MemInstToStreamMap.end()) {
      return false;
    }
    assert(!Iter->second.empty() &&
           "There should be at least one stream for base load.");
    const auto &BaseLoadPattern =
        BaseIter->second.front().getPattern().getPattern();
    switch (BaseLoadPattern.CurrentPattern) {
    case MemoryAccessPattern::Pattern::CONSTANT:
    case MemoryAccessPattern::Pattern::QUARDRIC:
    case MemoryAccessPattern::Pattern::LINEAR: {
      // Indirect stream based on linear/quardric stream.
      return true;
    }
    default: { return false; }
    }
    return false;
  }
  default: { return false; }
  }

  return false;
}

void StreamPass::computeStreamStatistics() {
  for (const auto &Entry : this->LoopTripCount) {
    auto Loop = Entry.first;
    auto Iters = Entry.second.first;
    auto Enters = Entry.second.second;
    auto LI = this->CachedLI->getLoopInfo(Loop->getHeader()->getParent());
    auto SE =
        this->CachedLI->getScalarEvolution(Loop->getHeader()->getParent());
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      if (LI->getLoopFor(BB) != Loop) {
        // This is not the closest loop for BB.
        continue;
      }
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        // We only care about memory accesses.
        if (!llvm::isa<llvm::LoadInst>(Inst) &&
            !llvm::isa<llvm::StoreInst>(Inst)) {
          continue;
        }

        bool IsLoad = llvm::isa<llvm::LoadInst>(Inst);
        llvm::Value *Addr = nullptr;
        if (IsLoad) {
          Addr = Inst->getOperand(0);
        } else {
          Addr = Inst->getOperand(1);
        }

        // Try to look at dynamic information.
        auto Iter = this->MemInstToStreamMap.find(Inst);
        if (Iter == this->MemInstToStreamMap.end()) {
          continue;
        }
        // We only look at the inner most loop level for now.
        assert(!Iter->second.empty() && "There should be at least one stream.");
        const auto &Pattern = Iter->second.front().getPattern().getPattern();

        const llvm::SCEV *SCEV = SE->getSCEV(Addr);

        if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
          if (IsLoad) {
            this->AddRecLoadCount.Val += Pattern.Accesses;
          } else {
            this->AddRecStoreCount.Val += Pattern.Accesses;
          }
          this->StreamCount.Val += Pattern.StreamCount;
          continue;
        }

        switch (Pattern.CurrentPattern) {
        case MemoryAccessPattern::Pattern::CONSTANT: {
          this->ConstantCount.Val += Pattern.Accesses;
          this->StreamCount.Val += Pattern.StreamCount;
          break;
        }
        case MemoryAccessPattern::Pattern::QUARDRIC: {
          this->QuardricCount.Val += Pattern.Accesses;
          this->StreamCount.Val += Pattern.StreamCount;
          break;
        }
        case MemoryAccessPattern::Pattern::LINEAR: {
          this->LinearCount.Val += Pattern.Accesses;
          this->StreamCount.Val += Pattern.StreamCount;
          break;
        }
        case MemoryAccessPattern::Pattern::INDIRECT: {
          auto BaseLoad = Pattern.BaseLoad;

          auto BaseIter = this->MemInstToStreamMap.find(BaseLoad);
          if (BaseIter == this->MemInstToStreamMap.end()) {
            break;
          }
          assert(!Iter->second.empty() &&
                 "There should be at least one stream for base load.");
          const auto &BaseLoadPattern =
              BaseIter->second.front().getPattern().getPattern();
          switch (BaseLoadPattern.CurrentPattern) {
          case MemoryAccessPattern::Pattern::CONSTANT:
          case MemoryAccessPattern::Pattern::QUARDRIC:
          case MemoryAccessPattern::Pattern::LINEAR: {
            // Indirect stream based on linear/quardric stream.
            this->IndirectCount.Val += Pattern.Accesses;
            this->StreamCount.Val += Pattern.StreamCount;
            break;
          }
          default: { break; }
          }
          break;
        }
        default: { break; }
        }
      }
    }
  }
}

void Stream::calculateAddressComputeInsts() {
  std::list<llvm::Instruction *> Queue;
  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->AddrValue)) {
    Queue.push_back(AddrInst);
  }
  while (!Queue.empty()) {
    auto Inst = Queue.front();
    Queue.pop_front();
    if (!this->Loop->contains(Inst)) {
      // This is not from our level of analysis, ignore it.
      continue;
    }
    if (this->AddressComputeInsts.count(Inst) != 0) {
      // We have already processed this one.
      continue;
    }
    switch (Inst->getOpcode()) {
    case llvm::Instruction::BitCast:
    case llvm::Instruction::Add:
    case llvm::Instruction::Sub:
    case llvm::Instruction::PHI:
    case llvm::Instruction::GetElementPtr: {
      this->AddressComputeInsts.insert(Inst);
      for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(
                Inst->getOperand(OperandIdx))) {
          Queue.push_back(OperandInst);
        }
      }
      break;
    }
    default: {
      // Ignore those unsupported instruction.
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
