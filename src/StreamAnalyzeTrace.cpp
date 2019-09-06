#include "trace/TraceParserGZip.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/CommandLine.h"

#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>

// NOT WORKING FOR NOW!!

static llvm::cl::opt<std::string> TraceFileName("stream-trace-file",
                                                llvm::cl::desc("Trace file."));

#define DEBUG_TYPE "ReplayPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

#define CCA_SUBGRAPH_DEPTH_MAX 7u

namespace {

struct CCASubGraph {
public:
  CCASubGraph() : Source(nullptr), InsertionPoint(nullptr) {}
  std::unordered_set<llvm::Instruction *> StaticInstructions;
  std::unordered_set<llvm::Instruction *> DependentStaticInstructions;
  std::unordered_set<llvm::Instruction *> UsedStaticInstructions;
  llvm::Instruction *Source;
  llvm::Instruction *InsertionPoint;
  void addInstruction(llvm::Instruction *StaticInstruction) {
    if (this->StaticInstructions.size() == 0) {
      // This is the first instruction.
      this->Source = StaticInstruction;
    }
    this->StaticInstructions.insert(StaticInstruction);
  }
  void removeInstruction(llvm::Instruction *StaticInstruction) {
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
    std::list<llvm::Instruction *> Stack;
    if (this->Source == nullptr) {
      return 0;
    }
    // Topological sort.
    std::unordered_map<llvm::Instruction *, int> Visited;
    Stack.push_back(this->Source);
    Visited[this->Source] = 0;
    std::list<llvm::Instruction *> Sorted;
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
    std::unordered_map<llvm::Instruction *, uint32_t> Dist;
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
      LLVM_DEBUG(llvm::errs() << "Subgraph " << Inst->getName() << '\n');
    }
  }
};
} // namespace

// Analyze the stream interface.
class StreamAnalysisTrace : public llvm::FunctionPass {
public:
  static char ID;
  StreamAnalysisTrace()
      : llvm::FunctionPass(ID), StaticMemAccessCount(0), StaticStreamCount(0),
        StaticAffineStreamCount(0), StaticQuadricStreamCount(0),
        DynamicInstructionCount(0), DynamicMemAccessCount(0),
        DynamicStreamCount(0), DynamicMemAccessBytes(0), DynamicStreamBytes(0),
        CurrentFunction(nullptr), CurrentBasicBlock(nullptr), CurrentIndex(-1),
        DataLayout(nullptr) {}
  virtual ~StreamAnalysisTrace() {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    // Not preserve the loop information.
    // Info.addPreserved<llvm::LoopInfoWrapperPass>();
    // We need ScalarEvolution.
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
    Info.addPreserved<llvm::ScalarEvolutionWrapperPass>();
    // We donot preserve the scev infomation as we may change function body.
  }

  bool doInitialization(llvm::Module &Module) override {
    assert(TraceFileName.getNumOccurrences() == 1 &&
           "Please specify the trace file.");

    this->DataLayout = new llvm::DataLayout(&Module);

    std::string TraceFileNameStr = TraceFileName;

    auto Parser = new TraceParserGZip(TraceFileNameStr);
    while (true) {
      auto NextType = Parser->getNextType();
      if (NextType == TraceParser::END) {
        break;
      }
      switch (NextType) {
      case TraceParser::INST: {
        auto Parsed = Parser->parseLLVMInstruction();
        this->parseInst(Module, Parsed);
        break;
      }
      case TraceParser::FUNC_ENTER: {
        auto Parsed = Parser->parseFunctionEnter();
        // Ignore function enter.
        break;
      }
      default: {
        llvm_unreachable("Unknown next type.");
        break;
      }
      }
    }
    delete Parser;

    for (size_t i = 0; i < CCA_SUBGRAPH_DEPTH_MAX; ++i) {
      this->SubGraphDepthHistograpm[i] = 0;
    }

    return false;
  }

  //   bool doInitialization(llvm::Module& Module) override;

  bool runOnFunction(llvm::Function &Function) override {
    // Collect information of stream in this function.
    // This will not modify the llvm function.
    llvm::ScalarEvolution &SE =
        this->getAnalysis<llvm::ScalarEvolutionWrapperPass>().getSE();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      this->CCASubGraphDepthAnalyze(&*BBIter);
      // for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
      //      InstIter != InstEnd; ++InstIter) {
      //   llvm::Instruction* Inst = &*InstIter;
      //   if (llvm::isa<llvm::LoadInst>(Inst) ||
      //       llvm::isa<llvm::StoreInst>(Inst)) {
      //     // This is a memory access we are about.
      //     this->StaticMemAccessCount++;

      //     llvm::Value* Addr = nullptr;
      //     if (llvm::isa<llvm::LoadInst>(Inst)) {
      //       Addr = Inst->getOperand(0);
      //     } else {
      //       Addr = Inst->getOperand(1);
      //     }
      //     uint64_t AccessBytes = this->DataLayout->getTypeStoreSize(
      //         Addr->getType()->getPointerElementType());
      //     if (this->StaticInstructionInstantiatedCount.find(Inst) !=
      //         this->StaticInstructionInstantiatedCount.end()) {
      //       this->DynamicMemAccessCount +=
      //           this->StaticInstructionInstantiatedCount.at(Inst);
      //       this->DynamicMemAccessBytes +=
      //           this->StaticInstructionInstantiatedCount.at(Inst) *
      //           AccessBytes;
      //     }
      //     // Check if this is a stream.
      //     const llvm::SCEV* SCEV = SE.getSCEV(Addr);
      //     std::string SCEVFormat;
      //     llvm::raw_string_ostream rs(SCEVFormat);
      //     SCEV->print(rs);
      //     //LLVM_DEBUG(llvm::errs() << rs.str() << '\n');

      //     //LLVM_DEBUG(SCEV->print(llvm::errs()));
      //     //LLVM_DEBUG(llvm::errs() << '\n');
      //     if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
      //       // This is a stream.
      //       this->StaticStreamCount++;
      //       if (AddRecSCEV->isAffine()) {
      //         rs << " affine";
      //         this->StaticAffineStreamCount++;
      //         if (this->StaticInstructionInstantiatedCount.find(Inst) !=
      //             this->StaticInstructionInstantiatedCount.end()) {
      //           auto DynamicInstructionCount =
      //               this->StaticInstructionInstantiatedCount.at(Inst);
      //           this->DynamicStreamCount += DynamicInstructionCount;
      //           this->DynamicStreamBytes +=
      //               AccessBytes * DynamicInstructionCount;
      //         }
      //       }
      //       if (AddRecSCEV->isQuadratic()) {
      //         this->StaticQuadricStreamCount++;
      //       }
      //     }

      //     this->StaticInstructionSCEVMap.emplace(Inst, rs.str());
      //   }
      // }
    }

    // Call the base runOnFunction to prepare it for replay.
    // For now no change.
    return false;
  }

  bool doFinalization(llvm::Module &Module) override {
    LLVM_DEBUG(llvm::errs() << "StaticMemAccessCount: "
                            << this->StaticMemAccessCount << '\n');
    LLVM_DEBUG(llvm::errs()
               << "StaticStreamCount: " << this->StaticStreamCount << '\n');
    LLVM_DEBUG(llvm::errs() << "StaticAffineStreamCount: "
                            << this->StaticAffineStreamCount << '\n');
    LLVM_DEBUG(llvm::errs() << "StaticQuadricStreamCount: "
                            << this->StaticQuadricStreamCount << '\n');
    LLVM_DEBUG(llvm::errs() << "DynamicInstructionCount: "
                            << this->DynamicInstructionCount << '\n');
    LLVM_DEBUG(llvm::errs() << "DynamicMemAccessCount: "
                            << this->DynamicMemAccessCount << '\n');
    LLVM_DEBUG(llvm::errs()
               << "DynamicStreamCount: " << this->DynamicStreamCount << '\n');
    LLVM_DEBUG(llvm::errs() << "DynamicMemAccessBytes: "
                            << this->DynamicMemAccessBytes << '\n');
    LLVM_DEBUG(llvm::errs()
               << "DynamicStreamBytes: " << this->DynamicStreamBytes << '\n');
    for (size_t i = 0; i < CCA_SUBGRAPH_DEPTH_MAX; ++i) {
      LLVM_DEBUG(llvm::errs() << "CCASubGraphDepth:" << i + 1 << ':'
                              << this->SubGraphDepthHistograpm[i] << '\n');
    }
    // Try to sort out top 10.
    // const size_t MaxCount = 50;
    // std::multimap<uint64_t, llvm::Instruction*> TopMemAccess;
    // for (const auto& Entry : this->StaticInstructionInstantiatedCount) {
    //   if (llvm::isa<llvm::LoadInst>(Entry.first) ||
    //       llvm::isa<llvm::StoreInst>(Entry.first)) {
    //     if (TopMemAccess.size() < MaxCount) {
    //       TopMemAccess.emplace(Entry.second, Entry.first);
    //     } else {
    //       uint64_t TemporaryMinimum = TopMemAccess.cbegin()->first;
    //       if (Entry.second > TemporaryMinimum) {
    //         TopMemAccess.emplace(Entry.second, Entry.first);
    //         // Remove the first (minimum) element.
    //         TopMemAccess.erase(TopMemAccess.cbegin());
    //       }
    //     }
    //   }
    // }
    // // Print out the top memory access.
    // for (auto& Entry : TopMemAccess) {
    //   llvm::Instruction* Inst = Entry.second;
    //  LLVM_DEBUG(llvm::errs() << Entry.first << ' ' <<
    //   Inst->getFunction()->getName()
    //                      << ' ' << Inst->getParent()->getName() << ' '
    //                      << Inst->getName() << ' '
    //                      << this->StaticInstructionSCEVMap.at(Inst) << '\n');
    // }

    delete this->DataLayout;
    this->DataLayout = nullptr;
    return false;
  }

protected:
  bool CCACheckConstraints(
      const CCASubGraph &SubGraph, llvm::Instruction *CandidateInst,
      const std::unordered_set<llvm::Instruction *> &MatchedInst) const {
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
    // return SubGraph.getDepth() <= CCAFUMax.getValue();
    return true;
  }

  void CCASubGraphDepthAnalyze(llvm::BasicBlock *BB) {
    auto FirstInst = &*(BB->begin());
    if (this->StaticInstructionInstantiatedCount.find(FirstInst) ==
        this->StaticInstructionInstantiatedCount.end()) {
      return;
    }
    auto InstantiateCount = StaticInstructionInstantiatedCount.at(FirstInst);

    CCASubGraph CurrentSubGraph;
    std::list<llvm::Instruction *> WaitList;
    std::unordered_set<llvm::Instruction *> MatchedStaticInsts;
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
        if (this->CCACheckConstraints(CurrentSubGraph, CandidateInst,
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
      LLVM_DEBUG(llvm::errs() << "Detect subgraph:\n");
      CurrentSubGraph.dump();
      auto Depth = CurrentSubGraph.getDepth();
      if (Depth > 0) {
        Depth = std::min(Depth, CCA_SUBGRAPH_DEPTH_MAX);
        for (size_t i = Depth - 1; i < CCA_SUBGRAPH_DEPTH_MAX; ++i) {
          this->SubGraphDepthHistograpm[i] += InstantiateCount;
        }
      }
    }
  }

  uint64_t StaticMemAccessCount;
  uint64_t StaticStreamCount;
  uint64_t StaticAffineStreamCount;
  uint64_t StaticQuadricStreamCount;

  uint64_t DynamicInstructionCount;
  uint64_t DynamicMemAccessCount;
  uint64_t DynamicStreamCount;
  uint64_t DynamicMemAccessBytes;
  uint64_t DynamicStreamBytes;

  // Hack this class for CCA analysis.
  uint64_t SubGraphDepthHistograpm[CCA_SUBGRAPH_DEPTH_MAX];

  std::unordered_map<llvm::Instruction *, uint64_t>
      StaticInstructionInstantiatedCount;

  std::unordered_map<llvm::Instruction *, std::string> StaticInstructionSCEVMap;

  llvm::Function *CurrentFunction;
  llvm::BasicBlock *CurrentBasicBlock;
  int64_t CurrentIndex;
  llvm::BasicBlock::iterator CurrentStaticInstruction;

  llvm::DataLayout *DataLayout;

  void parseInst(llvm::Module &Module, TraceParser::TracedInst &Parsed) {
    if (this->CurrentFunction == nullptr ||
        this->CurrentFunction->getName() != Parsed.Func) {
      this->CurrentFunction = Module.getFunction(Parsed.Func);
      assert(this->CurrentFunction &&
             "Failed to loop up traced function in module.");
      this->CurrentBasicBlock = nullptr;
      this->CurrentIndex = -1;
    }

    if (this->CurrentBasicBlock == nullptr ||
        Parsed.BB != this->CurrentBasicBlock->getName()) {
      // We are in a new basic block.
      // Find the new basic block.
      for (auto BBIter = this->CurrentFunction->begin(),
                BBEnd = this->CurrentFunction->end();
           BBIter != BBEnd; ++BBIter) {
        if (std::string(BBIter->getName()) == Parsed.BB) {
          this->CurrentBasicBlock = &*BBIter;
          // Clear index.
          this->CurrentIndex = -1;
          break;
        }
      }
    }

    assert(this->CurrentBasicBlock->getName() == Parsed.BB &&
           "Invalid basic block name.");

    if (Parsed.Id != this->CurrentIndex) {
      int Count = 0;
      for (auto InstIter = this->CurrentBasicBlock->begin(),
                InstEnd = this->CurrentBasicBlock->end();
           InstIter != InstEnd; ++InstIter) {
        if (Count == Parsed.Id) {
          this->CurrentIndex = Parsed.Id;
          this->CurrentStaticInstruction = InstIter;
          break;
        }
        Count++;
      }
    }

    llvm::Instruction *StaticInstruction = &*(this->CurrentStaticInstruction);

    assert(Parsed.Id == this->CurrentIndex && "Invalid index.");

    assert(this->CurrentStaticInstruction != this->CurrentBasicBlock->end() &&
           "Invalid end index.");

    this->CurrentStaticInstruction++;
    this->CurrentIndex++;
    if (this->StaticInstructionInstantiatedCount.find(StaticInstruction) ==
        this->StaticInstructionInstantiatedCount.end()) {
      this->StaticInstructionInstantiatedCount.emplace(StaticInstruction, 1);
    } else {
      this->StaticInstructionInstantiatedCount[StaticInstruction]++;
    }
    this->DynamicInstructionCount++;
  }
};

#undef DEBUG_TYPE
char StreamAnalysisTrace::ID = 0;
static llvm::RegisterPass<StreamAnalysisTrace>
    R("stream-analysis-trace", "analyze the llvm trace with stream inferface",
      false, false);