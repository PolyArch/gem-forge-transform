#include "Replay.h"

#include "llvm/Support/CommandLine.h"

#include <list>
#include <unordered_map>
#include <unordered_set>

static llvm::cl::opt<uint32_t> CCAFUMax("cca-max-fus",
                                        llvm::cl::desc("Number of FUs in CCA."),
                                        llvm::cl::init(7));

#define DEBUG_TYPE "ReplayPass"

struct CCASubGraph {
 public:
  CCASubGraph() : Source(nullptr), InsertionPoint(nullptr) {}
  std::unordered_set<llvm::Instruction*> StaticInstructions;
  std::unordered_set<llvm::Instruction*> DependentStaticInstructions;
  std::unordered_set<llvm::Instruction*> UsedStaticInstructions;
  llvm::Instruction* Source;
  llvm::Instruction* InsertionPoint;
  void addInstruction(llvm::Instruction* StaticInstruction) {
    if (this->StaticInstructions.size() == 0) {
      // This is the first instruction.
      this->Source = StaticInstruction;
    }
    this->StaticInstructions.insert(StaticInstruction);
  }
  void removeInstruction(llvm::Instruction* StaticInstruction) {
    this->StaticInstructions.erase(StaticInstruction);
    if (this->Source == StaticInstruction) {
      this->Source = nullptr;
    }
  }
  // Finally popluate the dependent and user.
  void finalize() {
    for (auto StaticInstruction : this->StaticInstructions) {
      // Add to dependent static instructions.
      for (unsigned OperandIdx = 0,
                    NumOperands = StaticInstruction->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto Operand = StaticInstruction->getOperand(OperandIdx);
        if (auto DependentStaticInstruction =
                llvm::dyn_cast<llvm::Instruction>(Operand)) {
          if (this->StaticInstructions.find(DependentStaticInstruction) ==
              this->StaticInstructions.end()) {
            this->DependentStaticInstructions.insert(
                DependentStaticInstruction);
          }
        }
      }
      // Add the used instructions.
      for (auto UserIter = StaticInstruction->user_begin(),
                UserEnd = StaticInstruction->user_end();
           UserIter != UserEnd; ++UserIter) {
        if (auto UsedStaticInstruction =
                llvm::dyn_cast<llvm::Instruction>(*UserIter)) {
          if (this->StaticInstructions.find(UsedStaticInstruction) ==
              this->StaticInstructions.end()) {
            this->UsedStaticInstructions.insert(UsedStaticInstruction);
          }
        }
      }
    }
  }
  uint32_t getDepth() const {
    std::list<llvm::Instruction*> Stack;
    if (this->Source == nullptr) {
      return 0;
    }
    // Topological sort.
    std::unordered_map<llvm::Instruction*, int> Visited;
    Stack.push_back(this->Source);
    Visited[this->Source] = 0;
    std::list<llvm::Instruction*> Sorted;
    while (!Stack.empty()) {
      auto Node = Stack.back();
      if (Visited.at(Node) == 0) {
        // We haven't visited this.
        Visited.at(Node) = 1;
        Sorted.push_back(Node);
        for (unsigned OperandIdx = 0, NumOperands = Node->getNumOperands();
             OperandIdx != NumOperands; ++OperandIdx) {
          auto Operand = Node->getOperand(OperandIdx);
          if (auto DepNode = llvm::dyn_cast<llvm::Instruction>(Operand)) {
            if (this->StaticInstructions.find(DepNode) !=
                this->StaticInstructions.end()) {
              // We found a dep node.
              auto VisitedIter = Visited.find(DepNode);
              if (VisitedIter == Visited.end() || VisitedIter->second == 0) {
                // We haven't visit this.
                Stack.push_back(DepNode);
                Visited[DepNode] = 0;
              }
            }
          }
        }
      } else {
        // We have visited this before.
        Stack.pop_back();
      }
    }
    std::unordered_map<llvm::Instruction*, uint32_t> Dist;
    Dist[this->Source] = 1;
    uint32_t Depth = 1;
    for (auto Node : Sorted) {
      for (unsigned OperandIdx = 0, NumOperands = Node->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto Operand = Node->getOperand(OperandIdx);
        if (auto DepNode = llvm::dyn_cast<llvm::Instruction>(Operand)) {
          if (this->StaticInstructions.find(DepNode) !=
              this->StaticInstructions.end()) {
            // We found a dep node.
            auto DistIter = Dist.find(DepNode);
            if (DistIter == Dist.end() ||
                DistIter->second < Dist.at(Node) + 1) {
              Dist[DepNode] = Dist.at(Node) + 1;
            }
            Depth = std::max(Depth, Dist.at(DepNode));
          }
        }
      }
    }
    return Depth;
  }
  void clear() {
    this->StaticInstructions.clear();
    this->DependentStaticInstructions.clear();
    this->UsedStaticInstructions.clear();
    this->InsertionPoint = nullptr;
    this->Source = nullptr;
  }
  void dump() const {
    for (auto Inst : this->StaticInstructions) {
      DEBUG(llvm::errs() << "Subgraph " << Inst->getName() << '\n');
    }
  }
};

class CCADynamicInstruction : public DynamicInstruction {
 public:
  CCADynamicInstruction(const std::string& _OpName, DynamicInstruction* _Prev,
                        DynamicInstruction* _Next)
      : DynamicInstruction(), OpName(_OpName) {
    this->Prev = _Prev;
    this->Next = _Next;
  }
  const std::string& getOpName() override { return this->OpName; }
  std::string OpName;
  // For debug.
  llvm::Instruction* InsertionPoint;
  std::unordered_set<llvm::Instruction*> ReplacedInsts;
  void dump() override {
    DEBUG(llvm::errs() << "CCA insertion point: " << InsertionPoint->getName()
                       << '\n');
  }
};

class CCA : public ReplayTrace {
 public:
  static char ID;
  CCA() : ReplayTrace(ID), ReplacedDynamicInsts(0) {}
  virtual ~CCA() {}

 protected:
  llvm::Instruction* determineInsertionPoint(
      const std::unordered_map<llvm::Instruction*, int>& Numbering,
      CCASubGraph& SubGraph) const {
    // Find the latest dependent.
    int Latest = -1;
    for (auto DependentInst : SubGraph.DependentStaticInstructions) {
      if (Numbering.find(DependentInst) == Numbering.end()) {
        continue;
      }
      auto DependentId = Numbering.at(DependentInst);
      if (DependentId > Latest) {
        // DEBUG(llvm::errs() << "Set Latest to " << DependentId << " of "
        //                    << DependentInst->getName() << '\n');
        Latest = DependentId;
      }
    }
    // Find the earliest usage.
    int Earliest = INT_MAX;
    for (auto UsedInst : SubGraph.UsedStaticInstructions) {
      if (Numbering.find(UsedInst) == Numbering.end()) {
        continue;
      }
      // Ignore the phi node.
      if (UsedInst->getOpcode() == llvm::Instruction::PHI) {
        continue;
      }
      Earliest = std::min(Numbering.at(UsedInst), Earliest);
    }

    llvm::Instruction* InsertionPoint = nullptr;
    // Find the insertion point.
    if (Latest < Earliest) {
      for (auto Inst : SubGraph.StaticInstructions) {
        if (Numbering.find(Inst) == Numbering.end()) {
          for (auto DumpInst : SubGraph.StaticInstructions) {
            DEBUG(llvm::errs() << "Failed to look up numbering for inst "
                               << DumpInst->getName() << '\n');
          }
          assert(false);
        }
        auto Idx = Numbering.at(Inst);
        if (Idx > Latest && Idx < Earliest) {
          // Found one.
          // DEBUG(llvm::errs() << "Found insertion point for subgraph "
          //                    << Inst->getName() << '\n');
          InsertionPoint = Inst;
          break;
        }
      }
    }
    SubGraph.InsertionPoint = InsertionPoint;
    return InsertionPoint;
  }

  std::unordered_map<llvm::Instruction*, int> getNumbering(
      llvm::BasicBlock* BB) const {
    std::unordered_map<llvm::Instruction*, int> Numbering;
    int Number = 0;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      Numbering.emplace(Inst, Number++);
    }
    return Numbering;
  }

  bool checkConstraints(
      const CCASubGraph& SubGraph, llvm::Instruction* CandidateInst,
      const std::unordered_set<llvm::Instruction*>& MatchedInst) const {
    switch (CandidateInst->getOpcode()) {
      case llvm::Instruction::PHI:
      case llvm::Instruction::Br:
      case llvm::Instruction::Switch:
      case llvm::Instruction::IndirectBr:
      case llvm::Instruction::Ret:
      case llvm::Instruction::Call:
      case llvm::Instruction::Load:
      case llvm::Instruction::Store: {
        return false;
      }
      default: { break; }
    }
    // Make sure that the candidate doesn't have dependence on other cca.
    // This is key to avoid lock between two cca instructions.
    // We do this by making sure that no cca is directly dependent on other cca.
    // This is pretty conservative.
    for (unsigned OperandIdx = 0, NumOperands = CandidateInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto Operand = CandidateInst->getOperand(OperandIdx);
      if (auto DepInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
        if (SubGraph.StaticInstructions.find(DepInst) ==
                SubGraph.StaticInstructions.end() &&
            MatchedInst.find(DepInst) != MatchedInst.end()) {
          // This dependent instruction is already replaced by some other cca.
          return false;
        }
      }
    }

    // Same thing for user.
    for (auto UserIter = CandidateInst->user_begin(),
              UserEnd = CandidateInst->user_end();
         UserIter != UserEnd; ++UserIter) {
      if (auto UsedStaticInstruction =
              llvm::dyn_cast<llvm::Instruction>(*UserIter)) {
        if (SubGraph.StaticInstructions.find(UsedStaticInstruction) ==
                SubGraph.StaticInstructions.end() &&
            MatchedInst.find(UsedStaticInstruction) != MatchedInst.end()) {
          return false;
        }
      }
    }

    // Check the number of instructions is within the number of
    // function units.
    // return SubGraph.StaticInstructions.size() <= CCAFUMax.getValue();
    return SubGraph.getDepth() <= CCAFUMax.getValue();
  }

  float estimateSpeedUp(const CCASubGraph& SubGraph) const {
    float TotalWeight = 0.0f;
    for (auto Inst : SubGraph.StaticInstructions) {
      switch (Inst->getOpcode()) {
        case llvm::Instruction::FAdd:
        case llvm::Instruction::FSub: {
          TotalWeight += 2.0f;
          break;
        }
        default: {
          TotalWeight += 1.0f;
          break;
        }
      }
    }
    if (TotalWeight > 3.0) {
      return 1.20;
    } else {
      return 1.00;
    }
  }

  std::list<CCASubGraph> detectSubGraph(llvm::BasicBlock* BB) {
    auto Numbering = this->getNumbering(BB);
    std::list<CCASubGraph> SubGraphs;
    CCASubGraph CurrentSubGraph;
    std::list<llvm::Instruction*> WaitList;
    std::unordered_set<llvm::Instruction*> MatchedStaticInsts;
    for (auto InstIter = BB->rbegin(), InstEnd = BB->rend();
         InstIter != InstEnd; ++InstIter) {
      auto Inst = &*InstIter;
      if (MatchedStaticInsts.find(Inst) != MatchedStaticInsts.end()) {
        continue;
      }
      // This is a new seed.
      CurrentSubGraph.clear();
      WaitList.clear();
      WaitList.push_back(Inst);
      while (!WaitList.empty()) {
        auto CandidateInst = WaitList.front();
        WaitList.pop_front();
        // Temporarily add the candidate into the subgraph.
        CurrentSubGraph.addInstruction(CandidateInst);
        if (this->checkConstraints(CurrentSubGraph, CandidateInst,
                                   MatchedStaticInsts)) {
          // This candidate is good to go.
          MatchedStaticInsts.insert(CandidateInst);
          // Add more dependent one into the wait list.
          for (unsigned OperandIdx = 0,
                        NumOperands = CandidateInst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            if (auto DependentInst = llvm::dyn_cast<llvm::Instruction>(
                    CandidateInst->getOperand(OperandIdx))) {
              if (MatchedStaticInsts.find(DependentInst) ==
                  MatchedStaticInsts.end()) {
                if (DependentInst->getParent() == BB) {
                  WaitList.push_back(DependentInst);
                }
              }
            }
          }
        } else {
          // This candidate breaks constraints, remove it.
          CurrentSubGraph.removeInstruction(CandidateInst);
        }
      }

      // We have a new subgraph.
      // First we finalize it.
      CurrentSubGraph.finalize();
      llvm::Instruction* InsertionPoint =
          this->determineInsertionPoint(Numbering, CurrentSubGraph);
      if (InsertionPoint != nullptr &&
          this->estimateSpeedUp(CurrentSubGraph) > 1.10) {
        // Take a copy of the current subgraph.
        DEBUG(llvm::errs() << "Detect subgraph:\n");
        CurrentSubGraph.dump();
        SubGraphs.emplace_back(CurrentSubGraph);
      } else {
        // Clear the matched inst in the current subgraph.
        for (auto Inst : CurrentSubGraph.StaticInstructions) {
          MatchedStaticInsts.erase(Inst);
        }
      }
    }

    return SubGraphs;
  }

  size_t getNumOccurrence(llvm::BasicBlock* BB) const {
    auto FirstInst = &(BB->front());
    auto Iter = this->Trace->StaticToDynamicMap.find(FirstInst);
    if (Iter == this->Trace->StaticToDynamicMap.end()) {
      return 0;
    } else {
      return Iter->second.size();
    }
  }

  void transferDependence(
      const std::unordered_set<DynamicInstruction*>& SubGraph,
      CCADynamicInstruction* CCADynamicInst, DynamicInstruction* ReplacedInst) {
    auto Iter = this->Trace->RegDeps.find(ReplacedInst);
    if (Iter != this->Trace->RegDeps.end()) {
      for (auto DynamicDepInst : Iter->second) {
        // If the dependent instruction belongs to the replaced subgraph,
        // we can sliently ignore this dependence.
        if (SubGraph.find(DynamicDepInst) != SubGraph.end()) {
          continue;
        }

        // Otherwise, copy the dependence to the new cca instruction.
        this->Trace->RegDeps.at(CCADynamicInst).insert(DynamicDepInst);
      }
    }
  }

  void replaceSubgraphs(llvm::BasicBlock* BB,
                        std::list<CCASubGraph>&& SubGraphs) {
    std::unordered_map<llvm::Instruction*, DynamicInstruction*>
        ReplacedStaticMap;
    size_t NumOccurrence = this->getNumOccurrence(BB);
    for (size_t i = 0; i < NumOccurrence; ++i) {
      ReplacedStaticMap.clear();
      // DEBUG(llvm::errs() << "Replacing occurrence #" << i << " / "
      //                    << NumOccurrence << '\n');
      for (auto& SubGraph : SubGraphs) {
        // Ignore those subgraph without insertion point.
        assert(SubGraph.InsertionPoint != nullptr &&
               "This subgraph doesn't have insertion point.");
        // Create the new dynamic inst.
        std::unordered_set<DynamicInstruction*> SubGraphDynamicInsts;
        auto CCADynamicInst =
            new CCADynamicInstruction("cca", nullptr, nullptr);
        CCADynamicInst->InsertionPoint = SubGraph.InsertionPoint;
        this->Trace->RegDeps.emplace(CCADynamicInst,
                                     std::unordered_set<DynamicInstruction*>());
        this->Trace->MemDeps.emplace(CCADynamicInst,
                                     std::unordered_set<DynamicInstruction*>());
        this->Trace->CtrDeps.emplace(CCADynamicInst,
                                     std::unordered_set<DynamicInstruction*>());

        // Collect all the dynamic instruction waiting to be replaced.
        // But do not modify the graph for now.
        bool Inserted = false;
        for (auto StaticInst : SubGraph.StaticInstructions) {
          //   DEBUG(llvm::errs()
          //         << "Processing inst " << StaticInst->getName() << '\n');
          assert(this->Trace->StaticToDynamicMap.find(StaticInst) !=
                     this->Trace->StaticToDynamicMap.end() &&
                 "Failed to find dynamic instruction to be replaced.");
          auto& DynamicInsts = this->Trace->StaticToDynamicMap.at(StaticInst);
          assert(DynamicInsts.size() == NumOccurrence - i &&
                 "Unmatched number of occurrence.");
          // Get the first dynamic insts.
          auto DynamicInst = DynamicInsts.front();
          DynamicInsts.pop_front();
          // Add the replaced map.
          this->ReplacedMap.emplace(DynamicInst, CCADynamicInst);
          ReplacedStaticMap.emplace(StaticInst, CCADynamicInst);
          SubGraphDynamicInsts.insert(DynamicInst);
        }

        // Now do the first job: fix the dependence.
        for (auto DynamicInst : SubGraphDynamicInsts) {
          llvm::Instruction* StaticInst = DynamicInst->getStaticInstruction();
          assert(StaticInst != nullptr &&
                 "This replaced dynamic instruction should be an llvm "
                 "instruction.");
          // Transfer any dependence.
          // DEBUG(llvm::errs() << "Transfering dynamic dependence for inst "
          //                    << StaticInst->getName() << '\n');
          this->transferDependence(SubGraphDynamicInsts, CCADynamicInst,
                                   DynamicInst);
          // DEBUG(llvm::errs() << "Transfering dynamic dependence for inst: "
          //                    << StaticInst->getName() << ": Done\n");
          // Remove any dependence.
          this->Trace->RegDeps.erase(DynamicInst);
          this->Trace->MemDeps.erase(DynamicInst);
          this->Trace->CtrDeps.erase(DynamicInst);
        }

        // Now do the second job: remove it from the list.
        for (auto DynamicInst : SubGraphDynamicInsts) {
          llvm::Instruction* StaticInst = DynamicInst->getStaticInstruction();
          assert(StaticInst != nullptr &&
                 "This replaced dynamic instruction should be an llvm "
                 "instruction.");
          // Remove the dynamic inst.
          // DEBUG(llvm::errs()
          //       << "Removing inst " << StaticInst->getName() << " from
          //       list\n");
          if (DynamicInst->Prev != nullptr) {
            DynamicInst->Prev->Next = DynamicInst->Next;
          } else {
            this->Trace->DynamicInstructionListHead = DynamicInst->Next;
          }
          if (DynamicInst->Next != nullptr) {
            DynamicInst->Next->Prev = DynamicInst->Prev;
          } else {
            this->Trace->DynamicInstructionListTail = DynamicInst->Prev;
          }
          // Check if this is the insertion point for the new cca inst.
          if (SubGraph.InsertionPoint == StaticInst) {
            // DEBUG(llvm::errs() << "Inserting cca inst in place of "
            //                    << StaticInst->getName() << '\n');
            assert(!Inserted &&
                   "The new CCA instruction has already been inserted.");
            CCADynamicInst->Prev = DynamicInst->Prev;
            CCADynamicInst->Next = DynamicInst->Next;
            if (CCADynamicInst->Prev != nullptr) {
              CCADynamicInst->Prev->Next = CCADynamicInst;
            } else {
              this->Trace->DynamicInstructionListHead = CCADynamicInst;
            }
            if (CCADynamicInst->Next != nullptr) {
              CCADynamicInst->Next->Prev = CCADynamicInst;
            } else {
              this->Trace->DynamicInstructionListTail = CCADynamicInst;
            }
            Inserted = true;
          }
        }

        assert(Inserted && "The new cca dynamic instruction is not inserted.");
        this->ReplacedDynamicInsts += SubGraph.StaticInstructions.size();
        // Finally release the memory.
        for (auto ReplacedDynamicInst : SubGraphDynamicInsts) {
          delete ReplacedDynamicInst;
        }
      }
    }
  }

  bool isBasicBlockTraced(llvm::BasicBlock* BB) const {
    auto FirstInst = &(BB->front());
    return this->Trace->StaticToDynamicMap.find(FirstInst) !=
           this->Trace->StaticToDynamicMap.end();
  }

  void CCATransform() {
    for (auto FuncIter = this->Module->begin(), FuncEnd = this->Module->end();
         FuncIter != FuncEnd; ++FuncIter) {
      if (FuncIter->isDeclaration()) {
        // Ignore function declaration.
        continue;
      }
      for (auto BBIter = FuncIter->begin(), BBEnd = FuncIter->end();
           BBIter != BBEnd; ++BBIter) {
        auto BB = &*BBIter;
        // Do we have trace of this BB?
        if (!this->isBasicBlockTraced(BB)) {
          continue;
        }
        auto SubGraphs = this->detectSubGraph(BB);
        DEBUG(llvm::errs() << "CCA Transforming on " << FuncIter->getName()
                           << "::" << BB->getName() << " Subgraphs "
                           << SubGraphs.size() << '\n');
        // Replace every thing.
        this->replaceSubgraphs(BB, std::move(SubGraphs));
      }
    }
    // Crazy final clean up.
    auto Iter = this->Trace->DynamicInstructionListHead;
    while (Iter != nullptr) {
      if (this->Trace->RegDeps.find(Iter) != this->Trace->RegDeps.end()) {
        auto& Deps = this->Trace->RegDeps.at(Iter);
        std::unordered_set<DynamicInstruction*> Replaced;
        for (auto DepIter = Deps.begin(), DepEnd = Deps.end();
             DepIter != DepEnd;) {
          if (this->ReplacedMap.find(*DepIter) != this->ReplacedMap.end()) {
            Replaced.insert(this->ReplacedMap.at(*DepIter));
            DepIter = Deps.erase(DepIter);
          } else {
            DepIter++;
          }
        }
        for (auto R : Replaced) {
          Deps.insert(R);
        }
      }
      Iter = Iter->Next;
    }
    DEBUG(llvm::errs() << "Replaced dynamic instructions "
                       << this->ReplacedDynamicInsts << '\n');
  }

  void TransformTrace() override {
    this->CCATransform();
    ReplayTrace::TransformTrace();
  }

 protected:
  uint64_t ReplacedDynamicInsts;
  std::unordered_map<DynamicInstruction*, DynamicInstruction*> ReplacedMap;
};

#undef DEBUG_TYPE

char CCA::ID = 0;
static llvm::RegisterPass<CCA> R(
    "replay-cca", "replay the llvm trace with cca transformation", false,
    false);