#include "DynamicLoopTree.h"
#include "MemoryAccessPattern.h"
#include "Replay.h"
#include "Utils.h"
#include "stream/InlineContext.h"
#include "stream/Stream.h"
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

// /**
//  * Stream class wraps over MemoryAccessPattern and contains the stream
//  pattern
//  * analyzed in a specific loop level.
//  */
// class Stream {
// public:
//   Stream(llvm::Loop *_Loop, llvm::Instruction *_MemInst)
//       : Loop(_Loop), MemInst(_MemInst), AddrValue(nullptr), Pattern(_MemInst)
//       {
//     assert(this->Loop->contains(this->MemInst) &&
//            "The loop should contain the memory access instruction.");

//     if (llvm::isa<llvm::LoadInst>(this->MemInst)) {
//       this->AddrValue = this->MemInst->getOperand(0);
//     } else {
//       this->AddrValue = this->MemInst->getOperand(1);
//     }

//     // Intialize the address computing instructions.
//     this->calculateAddressComputeInsts();
//   }

//   void addAccess(uint64_t Addr) { this->Pattern.addAccess(Addr); }
//   void addMissingAccess() { this->Pattern.addMissingAccess(); }
//   void endStream() { this->Pattern.endStream(); }
//   void finalizePattern() { this->Pattern.finalizePattern(); }

//   llvm::Loop *getLoop() const { return this->Loop; }
//   llvm::Instruction *getMemInst() const { return this->MemInst; }
//   llvm::Value *getAddrValue() const { return this->AddrValue; }
//   const MemoryAccessPattern &getPattern() const { return this->Pattern; }
//   const std::set<llvm::Instruction *> &getAddressComputeInsts() const {
//     return this->AddressComputeInsts;
//   }

//   // API related to indirect streams.
//   bool isIndirect() const { return !this->BaseLoads.empty(); }
//   bool isPointerChase() const {
//     if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(this->MemInst)) {
//       return this->BaseLoads.count(LoadInst) != 0;
//     } else {
//       return false;
//     }
//   }
//   size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
//   const std::set<llvm::LoadInst *> &getBaseLoads() const {
//     return this->BaseLoads;
//   }
//   size_t getNumAddressComputeInstructions() const {
//     return this->AddressComputeInsts.size();
//   }

// private:
//   llvm::Loop *Loop;
//   llvm::Instruction *MemInst;
//   llvm::Value *AddrValue;
//   MemoryAccessPattern Pattern;
//   std::set<llvm::Instruction *> AddressComputeInsts;
//   std::set<llvm::LoadInst *> BaseLoads;

//   /**
//    * Calculate the address compute instructions.
//    * This is done via BFS starting from the address value.
//    * Terminate at instructions:
//    * 1. Outside the current loop.
//    * 2. Phi node at the header of the loop (so that we only analyze the
//    current
//    * iteration).
//    * 3. Call/Invoke instruction.
//    * 4. Load instruction.
//    */
//   void calculateAddressComputeInsts();
// };

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

    // this->State = SEARCHING;
    // this->LoopTree = nullptr;

    this->MemorizedMemoryAccess.clear();

    return Ret;
  }

  bool finalize(llvm::Module &Module) override {

    // this->State = SEARCHING;
    // if (this->LoopTree != nullptr) {
    //   delete this->LoopTree;
    //   this->LoopTree = nullptr;
    // }

    return ReplayTrace::finalize(Module);
  }

  void dumpStats(std::ostream &O) override;

  void transform() override;

  /**
   * A map from ContextLoop to a list of active streams that is being analyzed
   * at that level of loop.
   * The list stores only reference, as the object is stored in InstStreamMap.
   */
  using ActiveStreamMap = std::unordered_map<ContextLoop, std::list<Stream *>>;

  /**
   * Analyze the stream pattern in the first pass.
   */
  void analyzeStream();

  void addAccess(const InlineContext &CurrentContext,
                 const std::list<ContextLoop> &LoopStack,
                 ActiveStreamMap &ActiveStreams,
                 DynamicInstruction *DynamicInst);
  void endIter(const std::list<ContextLoop> &LoopStack,
               ActiveStreamMap &ActiveStreams);
  void pushLoopStack(const InlineContext &CurrentContext,
                     std::list<ContextLoop> &LoopStack,
                     ActiveStreamMap &ActiveStreams,
                     const ContextLoop &NewContextLoop);
  void popLoopStack(std::list<ContextLoop> &LoopStack,
                    ActiveStreamMap &ActiveStreams);
  void activateStream(ActiveStreamMap &ActiveStreams, const ContextInst &CInst);

  bool initializeStreamIfNecessary(const std::list<ContextLoop> &LoopStack,
                                   const ContextInst &CInst);

  // /**
  //  * Replace the memory access with our stream interface in the second pass.
  //  */
  // void transformStream();

  // bool isLoopStream(llvm::Loop *Loop);

  // /**
  //  * Initialize the MemInstToStreamMap and MemorizedMemoryAccess.
  //  */
  // void initializeLoopStream(llvm::Loop *Loop);

  /**
   * For all the analyzed stream, choose the loop level for optimization.
   */
  void chooseStream();

  // void computeMemoryAccessPattern(DynamicLoopIteration *LoopTree);

  void computeStreamStatistics();

  std::string classifyStream(const Stream &S) const;

  // enum {
  //   SEARCHING,
  //   STREAMING,
  // } State;

  ProfileParser Profile;

  /**
   * A map from ContextInst to a list of streams.
   * Each stream contains the analysis result of the ContextInst in different
   * loop levels. Sorted from inner most loop to outer loops.
   */
  std::unordered_map<ContextInst, std::list<Stream>> InstStreamMap;

  enum LoopStatusT {
    CONTINUOUS,
    INLINE_CONTINUOUS,
    INCONTINUOUS,
    RECURSIVE,
  };

  static std::string formatLoopStatusT(const LoopStatusT Status) {
    switch (Status) {
    case CONTINUOUS:
      return "CONTINUOUS";
    case INLINE_CONTINUOUS:
      return "INLINE_CONTINUOUS";
    case INCONTINUOUS:
      return "INCONTINUOUS";
    case RECURSIVE:
      return "RECURSIVE";
    default: {
      llvm_unreachable("Illegal LoopStatusT for formating.");
      break;
    }
    }
  }

  static LoopStatusT initLoopStatusT(const LoopStatusT NestedLoopStatus) {
    if (NestedLoopStatus == RECURSIVE) {
      return RECURSIVE;
    } else {
      return CONTINUOUS;
    }
  }

  static LoopStatusT updateLoopStatusT(const LoopStatusT CurrentStatus,
                                       const LoopStatusT NewStatus) {
    return CurrentStatus > NewStatus ? CurrentStatus : NewStatus;
  }

  /**
   * A map from ContextLoop to its status.
   * Currently the status can be CONTINUOUS, INCONTINUOUS, RECURSIVE.
   * TODO: Rewrite this part.
   */
  std::unordered_map<ContextLoop, LoopStatusT> LoopStatus;

  // /**
  //  * A map from memory access instruction to a list of streams.
  //  * Each stream contains the analyzation result of the instruction in
  //  different
  //  * loop levels.
  //  */
  // std::unordered_map<llvm::Instruction *, std::list<Stream>>
  // MemInstToStreamMap;

  // /**
  //  * A map from memory access instruction to the chosen stream at a
  //  specific
  //  * loop level. The stream is actually stored in MemInstToStreamMap. Be
  //  careful
  //  * with the life span.
  //  */
  // std::unordered_map<llvm::Instruction *, Stream *>
  // MemInstToChosenStreamMap;

  // /**
  //  * A set contains the removed address compute intsruction.
  //  */
  // std::unordered_set<llvm::Instruction *>
  // RemovableAddrComputeInstSet;

  // DynamicLoopIteration *LoopTree;

  // std::unordered_map<llvm::Loop *, bool> MemorizedLoopStream;
  std::unordered_map<llvm::Loop *, std::unordered_set<llvm::Instruction *>>
      MemorizedMemoryAccess;

  // std::unordered_map<llvm::Loop *, size_t> LoopOngoingIters;

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

  // A map to store the all the memory access instruction.
  std::unordered_map<ContextInst, uint64_t> MemAccessInstCount;
}; // namespace

std::string StreamPass::classifyStream(const Stream &S) const {

  // First check the loop status.
  const auto &CLoop = S.getContextLoop();
  auto Status = this->LoopStatus.at(CLoop);
  if (Status == RECURSIVE) {
    return "RECURSIVE";
  } else if (Status == INCONTINUOUS) {
    return "INCONTINUOUS";
  }

  // The loop is continuous or inline continous.
  // We can further classify the stream now.
  if (S.getNumBaseLoads() > 1) {
    return "MULTI_BASE";
  } else if (S.getNumBaseLoads() == 1) {
    const auto &BaseLoad = *S.getBaseLoads().begin();
    if (BaseLoad == S.getContextInst()) {
      // Chasing myself.
      return "POINTER_CHASE";
    }
    const auto &BaseStreams = this->InstStreamMap.at(BaseLoad);
    for (const auto &BaseStream : BaseStreams) {
      if (BaseStream.getContextLoop() == S.getContextLoop()) {
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

  /**
   * Loop Inst AddrPat AccPat Iters Accs Updates StreamCount BaseLoads
   * StreamClass Footprint AddrInsts Chosen
   */

  O << "--------------- Stream -----------------\n";
  for (auto &MemInstStreamEntry : this->InstStreamMap) {
    for (auto &Stream : MemInstStreamEntry.second) {
      O << Stream.getContextLoop().format();
      O << ' ' << Stream.getContextInst().format();

      // std::string StreamClass = this->classifyStream(Stream);

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = Stream.getTotalIters();
      size_t Accesses = Stream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = Stream.getTotalStreams();
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
      O << ' ' << BaseLoads;
      O << ' ' << StreamClass;
      O << ' ' << Footprint;
      O << ' ' << AddrInsts;
      O << ' ' << Chosen;
      O << '\n';
    }
  }

  // Also dump statistics for all the unanalyzed memory accesses.
  for (const auto &Entry : this->MemAccessInstCount) {
    const auto &CInst = Entry.first;
    if (this->InstStreamMap.count(CInst) != 0) {
      // This instruction is already dumped.
      if (!this->InstStreamMap.at(CInst).empty()) {
        continue;
      }
    }
    std::string LoopId = "UNKNOWN";
    auto LI = this->CachedLI->getLoopInfo(
        const_cast<llvm::Function *>(CInst.Inst->getFunction()));
    auto Loop = LI->getLoopFor(CInst.Inst->getParent());
    if (Loop != nullptr) {
      LoopId = LoopUtils::getLoopId(Loop);
    }
    O << LoopId;
    O << ' ' << CInst.format(); // Inst
    O << ' ' << "NOT_STREAM";   // Address pattern
    O << ' ' << "NOT_STREAM";   // Access pattern
    O << ' ' << 0;              // Iters
    O << ' ' << Entry.second;   // Accesses
    O << ' ' << 0;              // Updates
    O << ' ' << 0;              // StreamCount
    O << ' ' << -1;             // BaseLoads
    O << ' ' << "NOT_STREAM";   // Stream class
    O << ' ' << 0;              // Footprint
    O << ' ' << -1;             // AddrInsts
    O << ' ' << "NO" << '\n';   // Chosen
  }

  O << "--------------- Loop -----------------\n";
  for (const auto &LoopStatusEntry : this->LoopStatus) {
    O << LoopStatusEntry.first.format() << ' '
      << StreamPass::formatLoopStatusT(LoopStatusEntry.second) << '\n';
  }
}

void StreamPass::transform() {
  // First pass.
  this->analyzeStream();

  for (auto &MemInstStreamEntry : this->InstStreamMap) {
    for (auto &Stream : MemInstStreamEntry.second) {
      // if (!Stream.getPattern().computed()) {
      //   llvm::errs() << LoopUtils::getLoopId(Stream.getLoop()) << " "
      //                << LoopUtils::formatLLVMInst(Stream.getMemInst())
      //                << " is not computed.\n";
      // } else {
      //   DEBUG(llvm::errs() << LoopUtils::getLoopId(Stream.getLoop()) << " "
      //                      << LoopUtils::formatLLVMInst(Stream.getMemInst())
      //                      << " is computed.\n");
      // }
      Stream.finalizePattern();
    }
  }

  // this->chooseStream();
  // this->computeStreamStatistics();

  // delete this->Trace;
  // this->Trace = new DataGraph(this->Module, this->DGDetailLevel);

  // Second pass.
  // this->transformStream();
}

// void StreamPass::chooseStream() {
//   // First pass to pick out direct streams.
//   for (auto &MemInstToStreamEntry : this->MemInstToStreamMap) {
//     Stream *ChosenStream = nullptr;
//     for (auto &Stream : MemInstToStreamEntry.second) {
//       if (Stream.isIndirect()) {
//         // Ignore indirect stream.
//         continue;
//       }
//       if (!Stream.getPattern().computed()) {
//         continue;
//       }
//       switch (Stream.getPattern().getPattern().CurrentPattern) {
//       case MemoryAccessPattern::Pattern::QUARDRIC:
//       case MemoryAccessPattern::Pattern::LINEAR: {
//         ChosenStream = &Stream;
//       }
//       default: { break; }
//       }
//     }
//     if (ChosenStream != nullptr) {
//       this->MemInstToChosenStreamMap.emplace(MemInstToStreamEntry.first,
//                                              ChosenStream);
//     }
//   }
//   // Second pass to make the indirect stream same loop level as the base
//   load. for (auto &MemInstToStreamEntry : this->MemInstToStreamMap) {
//     Stream *ChosenStream = nullptr;
//     for (auto &S : MemInstToStreamEntry.second) {
//       if (!S.isIndirect()) {
//         continue;
//       }
//       if (!S.getPattern().computed()) {
//         continue;
//       }

//       Stream *BaseStream = &S;
//       bool FoundBaseStream = false;
//       std::set<Stream *> SeenStreams;
//       SeenStreams.insert(BaseStream);
//       while (true) {
//         if (!BaseStream->isIndirect()) {
//           FoundBaseStream = true;
//           break;
//         }
//         if (BaseStream->getNumBaseLoads() != 1) {
//           break;
//         }
//         auto BaseLoad = *BaseStream->getBaseLoads().begin();
//         auto Iter = this->MemInstToStreamMap.find(BaseLoad);
//         if (Iter == this->MemInstToStreamMap.end()) {
//           break;
//         }
//         bool FoundNext = false;
//         for (auto &SS : Iter->second) {
//           // Make sure the next stream is in the same level of loop and also
//           is
//           // a new stream to avoid linked list infinite loop.
//           if (SS.getLoop() == S.getLoop() && SeenStreams.count(&SS) == 0) {
//             // We found the next base stream.
//             FoundNext = true;
//             BaseStream = &SS;
//             SeenStreams.insert(BaseStream);
//             break;
//           }
//         }
//         if (!FoundNext) {
//           break;
//         }
//       }

//       if (FoundBaseStream) {
//         auto Iter =
//             this->MemInstToChosenStreamMap.find(BaseStream->getMemInst());
//         if (Iter != this->MemInstToChosenStreamMap.end()) {
//           if (Iter->second->getLoop() == S.getLoop()) {
//             // We found the match.
//             // No need to search for next one.
//             ChosenStream = &S;
//             break;
//           }
//         }
//       }
//     }
//     if (ChosenStream != nullptr) {
//       this->MemInstToChosenStreamMap.emplace(MemInstToStreamEntry.first,
//                                              ChosenStream);
//     }
//   }
//   // Finally try to find the removable address compute instruction.
//   // For now I take a very conservative approach: only look at the address
//   value
//   // and see if that is within the chosen loop level and also has only one
//   user. for (const auto &MemInstToChosenStreamEntry :
//        this->MemInstToChosenStreamMap) {
//     const auto &ChosenStream = MemInstToChosenStreamEntry.second;
//     if (auto AddrInst =
//             llvm::dyn_cast<llvm::Instruction>(ChosenStream->getAddrValue()))
//             {
//       if (!ChosenStream->getLoop()->contains(AddrInst)) {
//         continue;
//       }
//       if (!AddrInst->hasOneUse()) {
//         continue;
//       }
//       // This is a removable address compute instruction.
//       this->RemovableAddrComputeInstSet.insert(AddrInst);
//     }
//   }
// }

// void StreamPass::analyzeStream() {
//   llvm::Loop *CurrentLoop = nullptr;

//   DEBUG(llvm::errs() << "Stream: Start analysis.\n");

//   while (true) {

//     auto NewInstIter = this->Trace->loadOneDynamicInst();

//     llvm::Instruction *NewStaticInst = nullptr;
//     llvm::Loop *NewLoop = nullptr;

//     bool IsAtHeadOfCandidate = false;

//     if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
//       // This is a new instruction.
//       NewStaticInst = (*NewInstIter)->getStaticInstruction();
//       assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

//       this->TracedDynInstCount.Val++;
//       if (llvm::isa<llvm::LoadInst>(NewStaticInst) ||
//           llvm::isa<llvm::StoreInst>(NewStaticInst)) {
//         this->DynMemInstCount.Val++;
//         if (this->MemAccessInstCount.count(NewStaticInst) == 0) {
//           this->MemAccessInstCount.emplace(NewStaticInst, 1);
//         } else {
//           this->MemAccessInstCount.at(NewStaticInst)++;
//         }
//       }

//       auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
//       NewLoop = LI->getLoopFor(NewStaticInst->getParent());

//       if (NewLoop != nullptr) {
//         if (this->isLoopStream(NewLoop)) {
//           IsAtHeadOfCandidate =
//               LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
//         } else {
//           NewLoop = nullptr;
//         }
//       }
//     } else {
//       // We at the end of the loop. Commit everything.
//       if (this->LoopTree != nullptr) {
//         this->LoopTree->addInstEnd(this->Trace->DynamicInstructionList.end());

//         this->computeMemoryAccessPattern(this->LoopTree);

//         delete this->LoopTree;
//         this->LoopTree = nullptr;
//       }
//       while (!this->Trace->DynamicInstructionList.empty()) {
//         this->Trace->commitOneDynamicInst();
//       }
//       break;
//     }

//     switch (this->State) {
//     case SEARCHING: {
//       assert(this->Trace->DynamicInstructionList.size() <= 2 &&
//              "For searching state, there should be at most 2 dynamic "
//              "instructions in the buffer.");

//       if (this->Trace->DynamicInstructionList.size() == 2) {
//         // We can commit the previous one.
//         this->Trace->commitOneDynamicInst();
//       }

//       if (IsAtHeadOfCandidate) {
//         // Time the start STREAMING.

//         // DEBUG(llvm::errs() << "Stream: Switch to STREAMING state.\n");

//         this->State = STREAMING;
//         CurrentLoop = NewLoop;
//         this->LoopTree = new DynamicLoopIteration(
//             NewLoop, this->Trace->DynamicInstructionList.end());
//         // Initialize the loop stream.
//         this->initializeLoopStream(CurrentLoop);
//         // Add the first instruction to the loop tree.
//         this->LoopTree->addInst(NewInstIter);

//         // DEBUG(llvm::errs() << "Stream: Switch to STREAMING state:
//         Done.\n");
//       }

//       break;
//     }
//     case STREAMING: {

//       // Add the newest instruction into the tree.
//       //   DEBUG(llvm::errs() << "Stream: Add instruction to loop tree.\n");
//       this->LoopTree->addInst(NewInstIter);
//       //   DEBUG(llvm::errs() << "Stream: Add instruction to loop tree:
//       //   Done.\n");

//       // Check if we have reached the number of iterations.
//       if (this->LoopTree->countIter() == 4 ||
//       !CurrentLoop->contains(NewLoop)) {

//         // Collect statistics in first pass.
//         this->computeMemoryAccessPattern(this->LoopTree);

//         auto NextIter = this->LoopTree->moveTailIter();

//         // Commit all the instructions.
//         while (this->Trace->DynamicInstructionList.size() > 1) {
//           this->Trace->commitOneDynamicInst();
//         }

//         // Clear our iters.
//         delete this->LoopTree;
//         this->LoopTree = NextIter;
//         if (this->LoopTree == nullptr) {
//           // We are not in the same loop.
//           //   DEBUG(llvm::errs() << "Stream: Out of the current STREAMING
//           //   loop.\n");
//           if (IsAtHeadOfCandidate) {
//             // We are in other candidate.
//             CurrentLoop = NewLoop;
//             this->LoopTree = new DynamicLoopIteration(
//                 NewLoop, this->Trace->DynamicInstructionList.end());
//             this->LoopTree->addInst(NewInstIter);
//           } else {
//             // We are not in any candidate.
//             // DEBUG(llvm::errs() << "Stream: Switch to SEARCHING state.\n");
//             this->State = SEARCHING;
//           }
//         } else {
//           // We are still in the same loop. Do nothing.
//         }
//       }

//       break;
//     }
//     default: {
//       llvm_unreachable("Stream: Invalid machine state.");
//       break;
//     }
//     }
//   }

//   DEBUG(llvm::errs() << "Stream: End.\n");
// }

// bool StreamPass::isLoopStream(llvm::Loop *Loop) {

//   assert(Loop != nullptr && "Loop should not be nullptr.");

//   auto Iter = this->MemorizedLoopStream.find(Loop);
//   if (Iter != this->MemorizedLoopStream.end()) {
//     return Iter->second;
//   }

//   // We allow nested loops.
//   bool IsLoopStream = true;
//   if (!LoopUtils::isLoopContinuous(Loop)) {
//     // DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
//     //                    << " is not stream as it is not continuous.\n");

//     IsLoopStream = false;
//   }

//   // Done: this loop can be represented as stream.
//   // DEBUG(llvm::errs() << "isLoopStream returned true.\n");
//   this->MemorizedLoopStream.emplace(Loop, IsLoopStream);
//   return IsLoopStream;
// }

// void StreamPass::initializeLoopStream(llvm::Loop *Loop) {
//   if (this->MemorizedMemoryAccess.find(Loop) !=
//       this->MemorizedMemoryAccess.end()) {
//     // We have alread initialized this loop stream.
//     return;
//   }

//   // First initialize MemorizedMemoryAccess for every subloop.
//   std::list<llvm::Loop *> Loops;
//   Loops.push_back(Loop);
//   while (!Loops.empty()) {
//     auto CurrentLoop = Loops.front();
//     Loops.pop_front();
//     if (this->MemorizedMemoryAccess.count(CurrentLoop) != 0) {
//       // This subloop is already processed.
//       continue;
//     }
//     this->MemorizedMemoryAccess.emplace(std::piecewise_construct,
//                                         std::forward_as_tuple(CurrentLoop),
//                                         std::forward_as_tuple());
//     for (auto SubLoop : CurrentLoop->getSubLoops()) {
//       Loops.push_back(SubLoop);
//     }
//   }

//   auto LI = this->CachedLI->getLoopInfo(Loop->getHeader()->getParent());
//   for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
//        BBIter != BBEnd; ++BBIter) {
//     auto BB = *BBIter;

//     auto InnerMostLoop = LI->getLoopFor(BB);

//     auto &MemAccess = this->MemorizedMemoryAccess.at(InnerMostLoop);

//     for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter !=
//     InstEnd;
//          ++InstIter) {
//       auto Inst = &*InstIter;
//       if ((!llvm::isa<llvm::LoadInst>(Inst)) &&
//           (!llvm::isa<llvm::StoreInst>(Inst))) {
//         continue;
//       }
//       // Insert the memory access instruction into the memorized loop.
//       MemAccess.insert(Inst);

//       // It is possible the memory instruction is already processed by some
//       // subloop.
//       auto &StreamList =
//           this->MemInstToStreamMap.emplace(Inst, std::list<Stream>())
//               .first->second;

//       // Initialize the stream for all loop level.
//       auto LoopLevel = InnerMostLoop;
//       auto StreamIter = StreamList.begin();
//       while (LoopLevel != Loop) {
//         if (StreamIter == StreamList.end()) {
//           StreamList.emplace_back(LoopLevel, Inst);
//         } else {
//           assert(StreamIter->getLoop() == LoopLevel && "Unmatched loop
//           level."); StreamIter++;
//         }
//         LoopLevel = LoopLevel->getParentLoop();
//       }
//       if (StreamIter == StreamList.end()) {
//         StreamList.emplace_back(LoopLevel, Inst);
//       } else {
//         assert(StreamIter->getLoop() == LoopLevel && "Unmatched loop
//         level."); StreamIter++;
//       }
//     }
//   }
// }

// void StreamPass::transformStream() {

//   // Grows at the end.
//   std::list<llvm::Loop *> RegionStack;

//   DEBUG(llvm::errs() << "Stream: Start Pass 2.\n");

//   while (true) {

//     auto NewInstIter = this->Trace->loadOneDynamicInst();

//     if (NewInstIter == this->Trace->DynamicInstructionList.end()) {
//       // We at the end of the loop. Commit everything.
//       while (!this->Trace->DynamicInstructionList.empty()) {
//         // Serialize in the second pass.
//         this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
//                                     this->Trace);
//         this->Trace->commitOneDynamicInst();
//       }
//       break;
//     }

//     // This is a new instruction.
//     auto NewDynamicInst = *NewInstIter;
//     auto NewStaticInst = NewDynamicInst->getStaticInstruction();
//     assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

//     auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
//     auto NewLoop = LI->getLoopFor(NewStaticInst->getParent());

//     bool IsAtHeadOfCandidate = false;

//     if (NewLoop != nullptr) {
//       if (this->isLoopStream(NewLoop)) {
//         IsAtHeadOfCandidate =
//             LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
//       } else {
//         NewLoop = nullptr;
//       }
//     }

//     // Pop all the exited region.
//     while (!RegionStack.empty() &&
//            !RegionStack.back()->contains(NewStaticInst)) {
//       RegionStack.pop_back();
//     }

//     // Enter new region.
//     if (NewLoop != nullptr && IsAtHeadOfCandidate) {
//       RegionStack.push_back(NewLoop);
//     }

//     // Do the transformation.
//     // Replace the stream load/store instruction.
//     // Remove the removable address compute instruction.
//     if (this->RemovableAddrComputeInstSet.count(NewStaticInst) != 0) {

//       // In order to maintain some sort of dependence chain, I will update
//       the
//       // register dependence look up table here.
//       // This works as this is the last instruction in the stream so far, so
//       the
//       // translateRegisterDependence will automatically pick up the
//       dependence. std::list<DynamicInstruction::DynamicId> RegDeps; for
//       (const auto &RegDep :
//            this->Trace->RegDeps.at(NewDynamicInst->getId())) {
//         RegDeps.push_back(RegDep.second);
//       }
//       this->Trace->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
//           NewStaticInst, std::move(RegDeps));

//       // Remove from the dependence map and so on.
//       this->Trace->commitDynamicInst(NewDynamicInst->getId());

//       // Remove from the list.
//       this->Trace->DynamicInstructionList.erase(NewInstIter);

//       this->RemovedAddrInstCount.Val++;

//     } else if (this->MemInstToChosenStreamMap.count(NewStaticInst) != 0) {
// // #define REPLACE_FOR_IDEAL_CACHE
// #ifdef REPLACE_FOR_IDEAL_CACHE
//       bool IsLoad = llvm::isa<llvm::LoadInst>(NewStaticInst);
//       DynamicInstruction *NewStreamInst = nullptr;
//       if (IsLoad) {
//         NewStreamInst = new StreamLoadInst(NewDynamicInst->getId());
//       } else {
//         NewStreamInst = new StreamStoreInst(NewDynamicInst->getId());
//       }

//       // Replace the original memory access with stream access.
//       *NewInstIter = NewStreamInst;

//       // Remember to delete the original one.
//       delete NewDynamicInst;
// #endif
//     }

//     // Serialize the instruction.
//     // Set a sufficient large buffer size so that the conditional branch
//     // instruction can find the next basic block.
//     if (this->Trace->DynamicInstructionList.size() == 100) {
//       this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
//                                   this->Trace);
//       this->Trace->commitOneDynamicInst();
//     }
//   }

//   DEBUG(llvm::errs() << "Stream: End.\n");
// }

// void StreamPass::computeMemoryAccessPattern(DynamicLoopIteration *LoopTree) {
//   assert(LoopTree != nullptr &&
//          "Null LoopTree when trying to compute memory access pattern.");

//   auto Loop = LoopTree->getLoop();
//   auto TripCount = LoopTree->countIter();

//   const auto &MemAccess = this->MemorizedMemoryAccess.at(Loop);

//   auto Iter = LoopTree;
//   for (size_t Trip = 0; Trip < TripCount; ++Trip) {

//     // Count for all the nested loop recursively.
//     for (auto NestIter = Loop->begin(), NestEnd = Loop->end();
//          NestIter != NestEnd; ++NestIter) {
//       auto ChildLoop = *NestIter;
//       this->computeMemoryAccessPattern(Iter->getChildIter(ChildLoop));
//     }

//     // Compute pattern for memory access within this loop.
//     for (auto MemAccessInst : MemAccess) {
//       auto DynamicInst = Iter->getDynamicInst(MemAccessInst);
//       auto &StreamList = this->MemInstToStreamMap.at(MemAccessInst);
//       if (DynamicInst == nullptr) {
//         // This memory access is not performed in this iteration.
//         for (auto &Stream : StreamList) {
//           Stream.addMissingAccess();
//         }
//       } else {
//         uint64_t Addr = 0;
//         if (llvm::isa<llvm::LoadInst>(MemAccessInst)) {
//           Addr = DynamicInst->DynamicOperands[0]->getAddr();
//         } else {
//           Addr = DynamicInst->DynamicOperands[1]->getAddr();
//         }
//         for (auto &Stream : StreamList) {
//           Stream.addAccess(Addr);
//         }
//       }
//     }

//     Iter = Iter->getNextIter();
//   }

//   if (Iter == nullptr) {
//     // This is the end of the loop, we should end the stream.
//     size_t PreviousIters = 0;
//     if (this->LoopOngoingIters.find(Loop) != this->LoopOngoingIters.end()) {
//       PreviousIters = this->LoopOngoingIters.at(Loop);
//     }
//     this->LoopOngoingIters.erase(Loop);
//     // We have to end the stream for all memory instruction with in the loop.
//     // TODO: Memorize this?
//     for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
//          BBIter != BBEnd; ++BBIter) {
//       auto BB = *BBIter;
//       for (auto InstIter = BB->begin(), InstEnd = BB->end();
//            InstIter != InstEnd; ++InstIter) {
//         auto Inst = &*InstIter;
//         if ((!llvm::isa<llvm::LoadInst>(Inst)) &&
//             (!llvm::isa<llvm::StoreInst>(Inst))) {
//           continue;
//         }
//         auto &StreamList = this->MemInstToStreamMap.at(Inst);
//         for (auto &Stream : StreamList) {
//           if (Stream.getLoop() == Loop) {
//             // DEBUG(llvm::errs()
//             //       << "Ending stream at loop " <<
//             LoopUtils::getLoopId(Loop)
//             //       << " inst " << LoopUtils::formatLLVMInst(Inst) << '\n');
//             // DEBUG(llvm::errs()
//             //       << "Inner most loop is "
//             //       << LoopUtils::getLoopId(
//             //              this->CachedLI->getLoopInfo(Inst->getFunction())
//             //                  ->getLoopFor(Inst->getParent()))
//             //       << '\n');
//             // DEBUG(llvm::errs()
//             //       << "Inst is memorized? " << MemAccess.count(Inst) <<
//             '\n'); Stream.endStream(); break;
//           }
//         }
//       }
//     }
//   } else {
//     // This loop is still going on.
//     size_t PreviousIters = 0;
//     if (this->LoopOngoingIters.find(Loop) != this->LoopOngoingIters.end()) {
//       PreviousIters = this->LoopOngoingIters.at(Loop);
//     }
//     this->LoopOngoingIters[Loop] = PreviousIters + TripCount;
//   }
// }

void StreamPass::computeStreamStatistics() {

  // for (const auto &ChosenStreamEntry : this->MemInstToChosenStreamMap) {
  //   auto ChosenStream = ChosenStreamEntry.second;
  //   if (!ChosenStream->getPattern().computed()) {
  //     continue;
  //   }

  //   const auto &Pattern = ChosenStream->getPattern().getPattern();
  //   if (ChosenStream->getNumBaseLoads() != 0) {
  //     // This is indirect stream.
  //     this->IndirectCount.Val += Pattern.Accesses;
  //     this->StreamCount.Val += Pattern.StreamCount;
  //     continue;
  //   }

  //   switch (Pattern.CurrentPattern) {
  //   case MemoryAccessPattern::Pattern::LINEAR: {
  //     this->LinearCount.Val += Pattern.Accesses;
  //     this->StreamCount.Val += Pattern.StreamCount;
  //     break;
  //   }
  //   case MemoryAccessPattern::Pattern::QUARDRIC: {
  //     this->QuardricCount.Val += Pattern.Accesses;
  //     this->StreamCount.Val += Pattern.StreamCount;
  //     break;
  //   }
  //   case MemoryAccessPattern::Pattern::CONSTANT: {
  //     this->ConstantCount.Val += Pattern.Accesses;
  //     this->StreamCount.Val += Pattern.StreamCount;
  //     break;
  //   }
  //   default: { break; }
  //   }

  //   // Check if this is an AddRec.
  //   auto Inst = ChosenStream->getMemInst();

  //   bool IsLoad = llvm::isa<llvm::LoadInst>(Inst);
  //   llvm::Value *Addr = nullptr;
  //   if (IsLoad) {
  //     Addr = Inst->getOperand(0);
  //   } else {
  //     Addr = Inst->getOperand(1);
  //   }

  //   auto SE = this->CachedLI->getScalarEvolution(Inst->getFunction());
  //   const llvm::SCEV *SCEV = SE->getSCEV(Addr);

  //   if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
  //     if (IsLoad) {
  //       this->AddRecLoadCount.Val += Pattern.Accesses;
  //     } else {
  //       this->AddRecStoreCount.Val += Pattern.Accesses;
  //     }
  //   }
  // }
}

// void Stream::calculateAddressComputeInsts() {
//   std::list<llvm::Instruction *> Queue;
//   if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(this->AddrValue)) {
//     Queue.push_back(AddrInst);
//   }
//   while (!Queue.empty()) {
//     auto Inst = Queue.front();
//     Queue.pop_front();
//     if (this->AddressComputeInsts.count(Inst) != 0) {
//       // We have already processed this one.
//       continue;
//     }
//     if (!this->Loop->contains(Inst)) {
//       // This is not from our level of analysis, ignore it.
//       continue;
//     }
//     if (llvm::isa<llvm::CallInst>(Inst) || llvm::isa<llvm::InvokeInst>(Inst))
//     {
//       // Ignore the call/invoke instruction.
//       continue;
//     }

//     this->AddressComputeInsts.insert(Inst);
//     if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
//       // This is a dependent load.
//       this->BaseLoads.insert(LoadInst);
//       // Do not go further for the load.
//       continue;
//     }

//     // BFS on the operands.
//     for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
//          OperandIdx != NumOperands; ++OperandIdx) {
//       if (auto OperandInst =
//               llvm::dyn_cast<llvm::Instruction>(Inst->getOperand(OperandIdx)))
//               {
//         Queue.push_back(OperandInst);
//       }
//     }
//   }
// }

bool StreamPass::initializeStreamIfNecessary(
    const std::list<ContextLoop> &LoopStack, const ContextInst &CInst) {
  // Check if this is the first time we encounter this access.
  auto Iter = this->InstStreamMap.find(CInst);
  if (Iter == this->InstStreamMap.end()) {
    // This is the first time.
    Iter = this->InstStreamMap.emplace(CInst, std::list<Stream>()).first;
    // Try to analyze the stream at all context loop.
    auto &StreamList = Iter->second;
    for (const auto &CLoop : LoopStack) {
      // Make sure that the inner most loop stays at the front of the list.
      StreamList.emplace_front(CInst, CLoop);
    }
    return true;
  }
  return false;
}

void StreamPass::activateStream(ActiveStreamMap &ActiveStreams,
                                const ContextInst &CInst) {
  auto InstStreamIter = this->InstStreamMap.find(CInst);
  assert(InstStreamIter != this->InstStreamMap.end() &&
         "Failed to look up streams to activate.");
  for (auto &S : InstStreamIter->second) {
    const auto &CLoop = S.getContextLoop();
    auto ActiveStreamsIter = ActiveStreams.find(CLoop);
    if (ActiveStreamsIter == ActiveStreams.end()) {
      ActiveStreamsIter =
          ActiveStreams.emplace(CLoop, std::list<Stream *>()).first;
    }
    bool AlreadyActive = false;
    for (const auto &ActiveStream : ActiveStreamsIter->second) {
      // Here actually we can compare by address.
      if ((ActiveStream) == (&S)) {
        AlreadyActive = true;
        break;
      }
    }
    if (!AlreadyActive) {
      ActiveStreamsIter->second.emplace_back(&S);
    }
  }
}

void StreamPass::pushLoopStack(const InlineContext &CurrentContext,
                               std::list<ContextLoop> &LoopStack,
                               ActiveStreamMap &ActiveStreams,
                               const ContextLoop &NewContextLoop) {

  // Compute the loop status for the new loop.
  auto NewLoopStatus = CONTINUOUS;
  if (!LoopStack.empty()) {
    const auto &PrevContextLoop = LoopStack.back();
    const auto &PrevLoopStatus = this->LoopStatus.at(PrevContextLoop);
    NewLoopStatus = StreamPass::initLoopStatusT(PrevLoopStatus);
  }
  // Manage the loop status.
  if (this->LoopStatus.count(NewContextLoop) == 0) {
    this->LoopStatus.emplace(NewContextLoop, NewLoopStatus);
  } else {
    this->LoopStatus.at(NewContextLoop) = StreamPass::updateLoopStatusT(
        this->LoopStatus.at(NewContextLoop), NewLoopStatus);
  }
  // DEBUG(llvm::errs() << "Push loop stack with status "
  //                    << StreamPass::formatLoopStatusT(
  //                           this->LoopStatus.at(NewContextLoop))
  //                    << '\n');

  LoopStack.emplace_back(NewContextLoop);
  // Iterate through all the memory access instructions at this level of loop.
  const auto &SubLoops = NewContextLoop.Loop->getSubLoops();
  for (auto BBIter = NewContextLoop.Loop->block_begin(),
            BBEnd = NewContextLoop.Loop->block_end();
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

    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if ((!llvm::isa<llvm::LoadInst>(StaticInst)) &&
          (!llvm::isa<llvm::StoreInst>(StaticInst))) {
        // This is not a memory access instruction.
        continue;
      }

      ContextInst CInst(CurrentContext, StaticInst);
      this->initializeStreamIfNecessary(LoopStack, CInst);
      this->activateStream(ActiveStreams, CInst);
    }
  }
}

void StreamPass::addAccess(const InlineContext &CurrentContext,
                           const std::list<ContextLoop> &LoopStack,
                           ActiveStreamMap &ActiveStreams,
                           DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  ContextInst CInst(CurrentContext, StaticInst);

  // Update the global count.
  {
    auto Iter = this->MemAccessInstCount.find(CInst);
    if (Iter == this->MemAccessInstCount.end()) {
      this->MemAccessInstCount.emplace(CInst, 1);
    } else {
      this->MemAccessInstCount.at(CInst)++;
    }
  }

  this->initializeStreamIfNecessary(LoopStack, CInst);
  this->activateStream(ActiveStreams, CInst);
  auto Iter = this->InstStreamMap.find(CInst);
  if (Iter == this->InstStreamMap.end()) {
    // This is possible as some times we have instructions not in a loop.
    return;
  }
  uint64_t Addr = 0;
  if (llvm::isa<llvm::LoadInst>(StaticInst)) {
    Addr = DynamicInst->DynamicOperands[0]->getAddr();
  } else {
    Addr = DynamicInst->DynamicOperands[1]->getAddr();
  }
  for (auto &Stream : Iter->second) {
    Stream.addAccess(Addr);
  }
}

void StreamPass::endIter(const std::list<ContextLoop> &LoopStack,
                         ActiveStreamMap &ActiveStreams) {
  assert(!LoopStack.empty() && "Empty loop stack when endIter is called.");
  const auto &EndedContextLoop = LoopStack.back();
  auto ActiveStreamIter = ActiveStreams.find(EndedContextLoop);
  if (ActiveStreamIter != ActiveStreams.end()) {
    for (auto &Stream : ActiveStreamIter->second) {
      // Only call endIter for those streams at my level.
      if (Stream->getContextLoop() == EndedContextLoop) {
        // This will implicitly call the addMissingAccess.
        Stream->endIter();
      }
    }
  }
}

void StreamPass::popLoopStack(std::list<ContextLoop> &LoopStack,
                              ActiveStreamMap &ActiveStreams) {
  // Poping from the loop stack automatically means that this iteration ends.
  assert(!LoopStack.empty() && "Empty loop stack when popLoopStack is called.");
  this->endIter(LoopStack, ActiveStreams);

  // Deactivate all the streams in this loop level.
  const auto &EndedContextLoop = LoopStack.back();
  auto ActiveStreamIter = ActiveStreams.find(EndedContextLoop);
  if (ActiveStreamIter != ActiveStreams.end()) {
    for (auto &ActiveStream : ActiveStreamIter->second) {
      ActiveStream->endStream();
    }
    ActiveStreams.erase(ActiveStreamIter);
  }

  // Pop from the loop stack.
  LoopStack.pop_back();
}

void StreamPass::analyzeStream() {

  DEBUG(llvm::errs() << "Stream: Start analysis.\n");

  std::list<ContextLoop> LoopStack;
  InlineContext CurrentContext;
  ActiveStreamMap ActiveStreams;
  llvm::Instruction *PrevCallInst = nullptr;

  while (true) {

    auto NewInstIter = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    bool IsAtHeadOfCandidate = false;
    bool IsMemAccess = false;

    // Commit at 1000 instructions limits.
    while (this->Trace->DynamicInstructionList.size() > 1000) {
      this->Trace->commitOneDynamicInst();
    }

    if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewInstIter)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

      this->TracedDynInstCount.Val++;
      IsMemAccess = llvm::isa<llvm::LoadInst>(NewStaticInst) ||
                    llvm::isa<llvm::StoreInst>(NewStaticInst);

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        IsAtHeadOfCandidate =
            LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
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

    /**
     * Handle the previous call/invoke instruction by checking the datagraph's
     * DynamicFrameStack. The datagraph has the exact information as it can
     * see FunctionEnter message in the trace, however that is hidden from
     * higher transform pass, so we check it by looking at the dynamic frame
     * stack.
     */
    // DEBUG(llvm::errs() << "Next inst is "
    //                    << LoopUtils::formatLLVMInst(NewStaticInst) << '\n');
    if (PrevCallInst != nullptr) {

      /**
       * To detect if the new inst is actually from the callee.
       * 1. If the new inst is not ret, then we can just compare the size
       *    of the context and the frame stack.
       * 2. Otherwise, getting a ret means that the frame stack is already
       * poped, we can simply check if the ret and the previous call are from
       * the same function.
       */
      bool IsCalleeTraced = false;
      if (llvm::isa<llvm::ReturnInst>(NewStaticInst)) {
        IsCalleeTraced =
            NewStaticInst->getFunction() != PrevCallInst->getFunction();
      } else {
        auto ContextSize = CurrentContext.size();
        auto DynamicFrameStackSize = this->Trace->DynamicFrameStack.size();
        IsCalleeTraced = ContextSize + 2 == DynamicFrameStackSize;
      }

      // DEBUG(llvm::errs() << "IsCalleeTraced " << IsCalleeTraced << '\n');

      if (IsCalleeTraced) {
        CurrentContext.push(PrevCallInst);
        for (const auto &CLoop : LoopStack) {
          auto &CurrentStatus = this->LoopStatus.at(CLoop);
          CurrentStatus =
              StreamPass::updateLoopStatusT(CurrentStatus, INLINE_CONTINUOUS);
        }
      } else {
        // Untraced calls, we have to check if we support this function.
        auto Callee = Utils::getCalledFunction(PrevCallInst);
        if (Callee != nullptr &&
            (LoopUtils::SupportedMathFunctions.count(Callee->getName()) ||
             Callee->isIntrinsic())) {
          // This is supported. We consider it as CONTINUOUS and do nothing.
        } else {
          // This is unsupported untraced call. We consider it as INCONTINUOUS.
          for (const auto &CLoop : LoopStack) {
            // DEBUG(llvm::errs() << "Update for INCONTINUOUS\n");
            auto &CurrentStatus = this->LoopStatus.at(CLoop);
            // DEBUG(llvm::errs() << "Get current status.\n");
            CurrentStatus =
                StreamPass::updateLoopStatusT(CurrentStatus, INCONTINUOUS);
            // DEBUG(llvm::errs()
            //       << "Update lop status with INCONTINUOUS for loop "
            //       << CLoop.format() << '\n');
          }
        }
      }
      PrevCallInst = nullptr;
    }

    // DEBUG(llvm::errs() << "Done handling previous call inst.\n");

    /**
     * Pop the loop stack.
     * First is the loop within the same function.
     */
    while (!LoopStack.empty()) {
      auto Loop = LoopStack.back().Loop;
      auto LoopFunc = Loop->getHeader()->getParent();
      if (LoopFunc == NewStaticInst->getFunction() &&
          !Loop->contains(NewStaticInst)) {
        // This loop is from the same function but not contains the new inst.
        this->popLoopStack(LoopStack, ActiveStreams);
      } else {
        // Either we poped to some upper function or the loop contains the new
        // inst.
        break;
      }
    }

    /**
     * Keep pop the loop stack if this is a ret instruction.
     * And also modify the InlineContext.
     */
    if (llvm::isa<llvm::ReturnInst>(NewStaticInst)) {
      // Pop the loops.
      while (!LoopStack.empty() &&
             LoopStack.back().Loop->contains(NewStaticInst)) {
        this->popLoopStack(LoopStack, ActiveStreams);
      }
      if (!CurrentContext.empty()) {
        // Pop one context call site.
        CurrentContext.pop();
      }
    }

    // DEBUG(llvm::errs() << "Done popping loop stack.\n");

    /**
     * Insert new loop if we are at the head.
     */
    if (NewLoop != nullptr && IsAtHeadOfCandidate) {
      if (LoopStack.empty() || LoopStack.back().Loop != NewLoop) {
        this->pushLoopStack(CurrentContext, LoopStack, ActiveStreams,
                            ContextLoop(CurrentContext, NewLoop));
      } else {
        // This means that we are at a new iteration.
        this->endIter(LoopStack, ActiveStreams);
      }
    }

    /**
     * If there is recursion, we have to mark all the context loops recursive.
     */
    if (CurrentContext.isRecursive(NewStaticInst->getFunction())) {
      for (const auto &ContextLoop : LoopStack) {
        auto &CurrentStatus = this->LoopStatus.at(ContextLoop);
        DEBUG(llvm::errs() << "This is recursive.\n");
        CurrentStatus = StreamPass::updateLoopStatusT(CurrentStatus, RECURSIVE);
      }
    }

    /**
     * If this is a call instruction, check if we can inline it.
     */
    if (Utils::isCallOrInvokeInst(NewStaticInst)) {
      // We do not know if the callee is actually traced.
      assert(PrevCallInst == nullptr &&
             "Unresolved previous call instruction.");
      PrevCallInst = NewStaticInst;
    }

    /**
     * If this is a memory access, call addAccess to update the stream.
     */
    if (IsMemAccess) {
      this->DynMemInstCount.Val++;
      this->addAccess(CurrentContext, LoopStack, ActiveStreams, *NewInstIter);
    }
  }
}

} // namespace

#undef DEBUG_TYPE

char StreamPass::ID = 0;
static llvm::RegisterPass<StreamPass> X("stream-pass", "Stream transform pass",
                                        false, false);