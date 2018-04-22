#include "DynamicTrace.h"
#include "GZUtil.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/CommandLine.h"

#include <list>
#include <map>
#include <unordered_map>

static llvm::cl::opt<std::string> TraceFileName("stream-trace-file",
                                                llvm::cl::desc("Trace file."));

#define DEBUG_TYPE "ReplayPass"

// Analyze the stream interface.
class StreamAnalysisTrace : public llvm::FunctionPass {
 public:
  static char ID;
  StreamAnalysisTrace()
      : llvm::FunctionPass(ID),
        StaticMemAccessCount(0),
        StaticStreamCount(0),
        StaticAffineStreamCount(0),
        StaticQuadricStreamCount(0),
        DynamicInstructionCount(0),
        DynamicMemAccessCount(0),
        DynamicStreamCount(0),
        DynamicMemAccessBytes(0),
        DynamicStreamBytes(0),
        CurrentFunction(nullptr),
        CurrentBasicBlock(nullptr),
        CurrentIndex(-1),
        DataLayout(nullptr) {}
  virtual ~StreamAnalysisTrace() {}

  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    // Not preserve the loop information.
    // Info.addPreserved<llvm::LoopInfoWrapperPass>();
    // We need ScalarEvolution.
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
    Info.addPreserved<llvm::ScalarEvolutionWrapperPass>();
    // We donot preserve the scev infomation as we may change function body.
  }

  bool doInitialization(llvm::Module& Module) override {
    assert(TraceFileName.getNumOccurrences() == 1 &&
           "Please specify the trace file.");

    this->DataLayout = new llvm::DataLayout(&Module);

    std::string TraceFileNameStr = TraceFileName;
    GZTrace* TraceFile = gzOpenGZTrace(TraceFileNameStr.c_str());
    assert(TraceFile != NULL && "Failed to open trace file.");
    std::string Line;
    std::list<std::string> LineBuffer;
    uint64_t LineCount = 0;
    // while (std::getline(TraceFile, Line)) {
    while (true) {
      int LineLen = gzReadNextLine(TraceFile);
      if (LineLen == 0) {
        // Reached the end.
        break;
      }

      assert(LineLen > 0 && "Something wrong when reading gz trace file.");

      // Copy the new line to "Line" variable.
      Line = TraceFile->LineBuf;
      LineCount++;
      assert(LineLen == Line.size() && "Unmatched LineLen with Line.size().");

      if (LineCount % 1000000 == 0) {
        DEBUG(llvm::errs() << "Read in line # " << LineCount << ": " << Line
                           << '\n');
      }

      if (DynamicTrace::isBreakLine(Line)) {
        // Time to parse the buffer.
        this->parseLineBuffer(Module, LineBuffer);
        // Clear the buffer.
        LineBuffer.clear();
      }
      // Add the new line into the buffer if not empty.
      if (Line.size() > 0) {
        LineBuffer.push_back(Line);
      }
    }
    // Process the last instruction if there is any.
    if (LineBuffer.size() != 0) {
      this->parseLineBuffer(Module, LineBuffer);
      LineBuffer.clear();
    }
    // TraceFile.close();
    gzCloseGZTrace(TraceFile);

    return false;
  }

  //   bool doInitialization(llvm::Module& Module) override;

  bool runOnFunction(llvm::Function& Function) override {
    // Collect information of stream in this function.
    // This will not modify the llvm function.
    llvm::ScalarEvolution& SE =
        this->getAnalysis<llvm::ScalarEvolutionWrapperPass>().getSE();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        llvm::Instruction* Inst = &*InstIter;
        if (llvm::isa<llvm::LoadInst>(Inst) ||
            llvm::isa<llvm::StoreInst>(Inst)) {
          // This is a memory access we are about.
          this->StaticMemAccessCount++;

          llvm::Value* Addr = nullptr;
          if (llvm::isa<llvm::LoadInst>(Inst)) {
            Addr = Inst->getOperand(0);
          } else {
            Addr = Inst->getOperand(1);
          }
          uint64_t AccessBytes = this->DataLayout->getTypeStoreSize(
              Addr->getType()->getPointerElementType());
          if (this->StaticInstructionInstantiatedCount.find(Inst) !=
              this->StaticInstructionInstantiatedCount.end()) {
            this->DynamicMemAccessCount +=
                this->StaticInstructionInstantiatedCount.at(Inst);
            this->DynamicMemAccessBytes +=
                this->StaticInstructionInstantiatedCount.at(Inst) * AccessBytes;
          }
          // Check if this is a stream.
          const llvm::SCEV* SCEV = SE.getSCEV(Addr);
          std::string SCEVFormat;
          llvm::raw_string_ostream rs(SCEVFormat);
          SCEV->print(rs);
          // DEBUG(llvm::errs() << rs.str() << '\n');

          // DEBUG(SCEV->print(llvm::errs()));
          // DEBUG(llvm::errs() << '\n');
          if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
            // This is a stream.
            this->StaticStreamCount++;
            if (AddRecSCEV->isAffine()) {
              rs << " affine";
              this->StaticAffineStreamCount++;
              if (this->StaticInstructionInstantiatedCount.find(Inst) !=
                  this->StaticInstructionInstantiatedCount.end()) {
                auto DynamicInstructionCount =
                    this->StaticInstructionInstantiatedCount.at(Inst);
                this->DynamicStreamCount += DynamicInstructionCount;
                this->DynamicStreamBytes +=
                    AccessBytes * DynamicInstructionCount;
              }
            }
            if (AddRecSCEV->isQuadratic()) {
              this->StaticQuadricStreamCount++;
            }
          }

          this->StaticInstructionSCEVMap.emplace(Inst, rs.str());
        }
      }
    }

    // Call the base runOnFunction to prepare it for replay.
    // For now no change.
    return false;
    // return ReplayTrace::runOnFunction(Function);
  }

  bool doFinalization(llvm::Module& Module) override {
    DEBUG(llvm::errs() << "StaticMemAccessCount: " << this->StaticMemAccessCount
                       << '\n');
    DEBUG(llvm::errs() << "StaticStreamCount: " << this->StaticStreamCount
                       << '\n');
    DEBUG(llvm::errs() << "StaticAffineStreamCount: "
                       << this->StaticAffineStreamCount << '\n');
    DEBUG(llvm::errs() << "StaticQuadricStreamCount: "
                       << this->StaticQuadricStreamCount << '\n');
    DEBUG(llvm::errs() << "DynamicInstructionCount: "
                       << this->DynamicInstructionCount << '\n');
    DEBUG(llvm::errs() << "DynamicMemAccessCount: "
                       << this->DynamicMemAccessCount << '\n');
    DEBUG(llvm::errs() << "DynamicStreamCount: " << this->DynamicStreamCount
                       << '\n');
    DEBUG(llvm::errs() << "DynamicMemAccessBytes: "
                       << this->DynamicMemAccessBytes << '\n');
    DEBUG(llvm::errs() << "DynamicStreamBytes: " << this->DynamicStreamBytes
                       << '\n');
    // Try to sort out top 10.
    const size_t MaxCount = 50;
    std::multimap<uint64_t, llvm::Instruction*> TopMemAccess;
    for (const auto& Entry : this->StaticInstructionInstantiatedCount) {
      if (llvm::isa<llvm::LoadInst>(Entry.first) ||
          llvm::isa<llvm::StoreInst>(Entry.first)) {
        if (TopMemAccess.size() < MaxCount) {
          TopMemAccess.emplace(Entry.second, Entry.first);
        } else {
          uint64_t TemporaryMinimum = TopMemAccess.cbegin()->first;
          if (Entry.second > TemporaryMinimum) {
            TopMemAccess.emplace(Entry.second, Entry.first);
            // Remove the first (minimum) element.
            TopMemAccess.erase(TopMemAccess.cbegin());
          }
        }
      }
    }
    // Print out the top memory access.
    for (auto& Entry : TopMemAccess) {
      llvm::Instruction* Inst = Entry.second;
      DEBUG(llvm::errs() << Entry.first << ' ' << Inst->getFunction()->getName()
                         << ' ' << Inst->getParent()->getName() << ' '
                         << Inst->getName() << ' '
                         << this->StaticInstructionSCEVMap.at(Inst) << '\n');
    }

    delete this->DataLayout;
    this->DataLayout = nullptr;
    return false;
  }

 protected:
  uint64_t StaticMemAccessCount;
  uint64_t StaticStreamCount;
  uint64_t StaticAffineStreamCount;
  uint64_t StaticQuadricStreamCount;

  uint64_t DynamicInstructionCount;
  uint64_t DynamicMemAccessCount;
  uint64_t DynamicStreamCount;
  uint64_t DynamicMemAccessBytes;
  uint64_t DynamicStreamBytes;

  std::unordered_map<llvm::Instruction*, uint64_t>
      StaticInstructionInstantiatedCount;

  std::unordered_map<llvm::Instruction*, std::string> StaticInstructionSCEVMap;

  llvm::Function* CurrentFunction;
  llvm::BasicBlock* CurrentBasicBlock;
  int64_t CurrentIndex;
  llvm::BasicBlock::iterator CurrentStaticInstruction;

  llvm::DataLayout* DataLayout;

  void parseLineBuffer(llvm::Module& Module,
                       const std::list<std::string>& LineBuffer) {
    // Silently ignore empty line buffer.
    if (LineBuffer.size() == 0) {
      return;
    }
    switch (LineBuffer.front().front()) {
      case 'i': {
        // This is a dynamic instruction.
        // Try to locate the static instruciton.
        auto InstructionLineFields =
            DynamicTrace::splitByChar(LineBuffer.front(), '|');
        assert(InstructionLineFields.size() == 5 &&
               "Incorrect fields for instrunction line.");
        assert(InstructionLineFields[0] == "i" &&
               "The first filed must be \"i\".");
        const std::string& FunctionName = InstructionLineFields[1];
        const std::string& BasicBlockName = InstructionLineFields[2];
        int Index = std::stoi(InstructionLineFields[3]);

        if (this->CurrentFunction == nullptr ||
            this->CurrentFunction->getName() != FunctionName) {
          this->CurrentFunction = Module.getFunction(FunctionName);
          assert(this->CurrentFunction &&
                 "Failed to loop up traced function in module.");
          this->CurrentBasicBlock = nullptr;
          this->CurrentIndex = -1;
        }

        if (this->CurrentBasicBlock == nullptr ||
            BasicBlockName != this->CurrentBasicBlock->getName()) {
          // We are in a new basic block.
          // Find the new basic block.
          for (auto BBIter = this->CurrentFunction->begin(),
                    BBEnd = this->CurrentFunction->end();
               BBIter != BBEnd; ++BBIter) {
            if (std::string(BBIter->getName()) == BasicBlockName) {
              this->CurrentBasicBlock = &*BBIter;
              // Clear index.
              this->CurrentIndex = -1;
              break;
            }
          }
        }

        assert(this->CurrentBasicBlock->getName() == BasicBlockName &&
               "Invalid basic block name.");

        if (Index != this->CurrentIndex) {
          int Count = 0;
          for (auto InstIter = this->CurrentBasicBlock->begin(),
                    InstEnd = this->CurrentBasicBlock->end();
               InstIter != InstEnd; ++InstIter) {
            if (Count == Index) {
              this->CurrentIndex = Index;
              this->CurrentStaticInstruction = InstIter;
              break;
            }
            Count++;
          }
        }

        llvm::Instruction* StaticInstruction =
            &*(this->CurrentStaticInstruction);

        assert(Index == this->CurrentIndex && "Invalid index.");

        assert(this->CurrentStaticInstruction !=
                   this->CurrentBasicBlock->end() &&
               "Invalid end index.");

        this->CurrentStaticInstruction++;
        this->CurrentIndex++;
        if (this->StaticInstructionInstantiatedCount.find(StaticInstruction) ==
            this->StaticInstructionInstantiatedCount.end()) {
          this->StaticInstructionInstantiatedCount.emplace(StaticInstruction,
                                                           1);
        } else {
          this->StaticInstructionInstantiatedCount[StaticInstruction]++;
        }
        this->DynamicInstructionCount++;
        break;
      }
      case 'e':
        // Simply ignore function enter trace.
        break;
      default:
        llvm_unreachable("Unrecognized line buffer.");
    }
  }
};

#undef DEBUG_TYPE
char StreamAnalysisTrace::ID = 0;
static llvm::RegisterPass<StreamAnalysisTrace> R(
    "stream-analysis-trace", "analyze the llvm trace with stream inferface",
    false, false);