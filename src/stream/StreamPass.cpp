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

class StreamPass : public ReplayTrace {
public:
  static char ID;

#define INIT_VAR(var) var(#var)
  StreamPass()
      : ReplayTrace(ID), INIT_VAR(DynInstCount), INIT_VAR(DynMemInstCount),
        INIT_VAR(TracedDynInstCount), INIT_VAR(AddRecLoadCount),
        INIT_VAR(AddRecStoreCount), INIT_VAR(StreamCount) {}
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
   * Replace the memory access with our stream interface.
   */
  void streamTransform();

  bool isLoopStream(llvm::Loop *Loop);

  bool isStreamAccess(llvm::ScalarEvolution *SE,
                      llvm::Instruction *StaticInst) const;

  void updateTripCount(DynamicLoopIteration *LoopTree);

  void computeMemoryAccessPattern(DynamicLoopIteration *LoopTree);

  void computeStreamStatistics();

  enum {
    SEARCHING,
    STREAMING,
  } State;

  ProfileParser Profile;

  MemoryAccessPattern MemPattern;

  DynamicLoopIteration *LoopTree;

  std::unordered_map<llvm::Loop *, bool> MemorizedLoopStream;
  std::unordered_map<llvm::Loop *, std::unordered_set<llvm::Instruction *>>
      MemorizedMemoryAccess;

  std::unordered_map<llvm::Loop *, std::pair<uint64_t, uint64_t>> LoopTripCount;

  /*******************************************************
   * Statistics.
   *******************************************************/
  ScalarUIntVar DynInstCount;
  ScalarUIntVar DynMemInstCount;
  ScalarUIntVar TracedDynInstCount;

  ScalarUIntVar AddRecLoadCount;
  ScalarUIntVar AddRecStoreCount;
  ScalarUIntVar StreamCount;
}; // namespace

void StreamPass::dumpStats(std::ostream &O) {
  O << "--------------- Stream -----------------\n";
  this->DynInstCount.print(O);
  this->DynMemInstCount.print(O);
  this->TracedDynInstCount.print(O);
  this->AddRecLoadCount.print(O);
  this->AddRecStoreCount.print(O);
  this->StreamCount.print(O);
  for (const auto &LoopMemAccess : this->MemorizedMemoryAccess) {
    for (auto MemAccessInst : LoopMemAccess.second) {
      const auto &Pattern = this->MemPattern.getPattern(MemAccessInst);
      O << LoopUtils::formatLLVMInst(MemAccessInst) << ": "
        << MemoryAccessPattern::formatPattern(Pattern.CurrentPattern) << '\n';
      O << LoopUtils::formatLLVMInst(MemAccessInst) << ": " << Pattern.Count
        << '\n';
      O << LoopUtils::formatLLVMInst(MemAccessInst) << ": "
        << Pattern.StreamCount << '\n';
    }
  }
}

void StreamPass::transform() {
  llvm::Loop *CurrentLoop = nullptr;

  DEBUG(llvm::errs() << "Stream: Start.\n");

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;

    bool IsAtHeadOfCandidate = false;

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewInstIter)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

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
        this->updateTripCount(this->LoopTree);
        this->computeMemoryAccessPattern(this->LoopTree);
        delete this->LoopTree;
        this->LoopTree = nullptr;
      }
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
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
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }

      if (IsAtHeadOfCandidate) {
        // Time the start STREAMING.

        // DEBUG(llvm::errs() << "Stream: Switch to STREAMING state.\n");

        this->State = STREAMING;
        CurrentLoop = NewLoop;
        this->LoopTree = new DynamicLoopIteration(
            NewLoop, this->Trace->DynamicInstructionList.end());
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

        // Collect statistics.
        this->updateTripCount(this->LoopTree);
        this->computeMemoryAccessPattern(this->LoopTree);

        auto NextIter = this->LoopTree->moveTailIter();

        // Do stream transform.
        this->streamTransform();

        // Commit all the instructions.
        while (this->Trace->DynamicInstructionList.size() > 1) {
          this->Serializer->serialize(
              this->Trace->DynamicInstructionList.front(), this->Trace);
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

  this->computeStreamStatistics();

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

void StreamPass::streamTransform() {
  assert(this->LoopTree != nullptr && "Empty loop tree for transformation.");

  auto Loop = this->LoopTree->getLoop();

  auto SE = this->CachedLI->getScalarEvolution(Loop->getHeader()->getParent());

  // Loop through all the iterations.
  auto CurrentTree = this->LoopTree;
  for (size_t CurrentIter = 0, TotalIter = this->LoopTree->countIter();
       CurrentIter != TotalIter;
       ++CurrentIter, CurrentTree = CurrentTree->getNextIter()) {
    assert(CurrentTree != nullptr &&
           "Empty current loop tree for transformation.");

    // Loop through all the instructions in this iteration.
    for (auto DynamicInstIter = CurrentTree->begin(),
              DynamicInstEnd = CurrentTree->end();
         DynamicInstIter != DynamicInstEnd; ++DynamicInstIter) {
      auto DynamicInst = *DynamicInstIter;
      auto StaticInst = DynamicInst->getStaticInstruction();
      assert(StaticInst != nullptr && "Non llvm static instruction found.");

      // Check if this is a stream access.
      if (this->isStreamAccess(SE, StaticInst)) {
        bool IsLoad = llvm::isa<llvm::LoadInst>(StaticInst);
        DynamicInstruction *NewStreamInst = nullptr;
        if (IsLoad) {
          NewStreamInst = new StreamLoadInst(DynamicInst->getId());
        } else {
          NewStreamInst = new StreamStoreInst(DynamicInst->getId());
        }

        // Replace the original memory access with stream access.
        *DynamicInstIter = NewStreamInst;

        // Remember to delete the original one.
        delete DynamicInst;
      }
    }
  }
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

  // Collect memory access for within this loop only.
  // Memorize this result.
  if (this->MemorizedMemoryAccess.find(Loop) ==
      this->MemorizedMemoryAccess.end()) {
    std::unordered_set<llvm::Instruction *> &MemAccess =
        this->MemorizedMemoryAccess
            .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                     std::forward_as_tuple())
            .first->second;
    auto LI = this->CachedLI->getLoopInfo(Loop->getHeader()->getParent());
    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      if (LI->getLoopFor(BB) != Loop) {
        // This BB is within some nested loop.
        continue;
      }
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        if (llvm::isa<llvm::LoadInst>(Inst) ||
            llvm::isa<llvm::StoreInst>(Inst)) {
          MemAccess.insert(Inst);
        }
      }
    }
  }
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
      if (DynamicInst == nullptr) {
        // TODO: This instruction is not executed in this iteration.
        // Should have a way to inform pattern computer here.
      } else {
        uint64_t Addr = 0;
        if (llvm::isa<llvm::LoadInst>(MemAccessInst)) {
          Addr = DynamicInst->DynamicOperands[0]->getAddr();
        } else {
          Addr = DynamicInst->DynamicOperands[1]->getAddr();
        }
        this->MemPattern.addAccess(MemAccessInst, Addr);
      }
    }

    Iter = Iter->getNextIter();
  }

  if (Iter == nullptr) {
    // This is the end of the loop, we should end the stream.
    for (auto MemAccessInst : MemAccess) {
      if (MemAccessInst->getParent()->getName() == "bb22") {
        DEBUG(llvm::errs() << "End loop for inst "
                           << LoopUtils::formatLLVMInst(MemAccessInst) << '\n');
      }
      this->MemPattern.endLoop(MemAccessInst);
    }
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
        if (!this->isStreamAccess(SE, Inst)) {
          continue;
        }
        bool IsLoad = llvm::isa<llvm::LoadInst>(Inst);
        // This is a stream.
        if (IsLoad) {
          this->AddRecLoadCount.Val += Iters;
        } else {
          this->AddRecStoreCount.Val += Iters;
        }
        this->StreamCount.Val += Enters;
      }
    }
  }
}

} // namespace

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);
