#include "Replay.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"

#include <unordered_map>

#define DEBUG_TYPE "ReplayPass"

// Analyze the stream interface.
class StreamReplayTrace : public ReplayTrace {
 public:
  static char ID;
  StreamReplayTrace()
      : StaticMemAccessCount(0),
        StaticStreamCount(0),
        StaticLengthDeterminableStreamCount(0),
        DynamicMemAccessCount(0),
        DynamicStreamCount(0) {}
  virtual ~StreamReplayTrace() {}

  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    ReplayTrace::getAnalysisUsage(Info);

    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    // Not preserve the loop information.
    // Info.addPreserved<llvm::LoopInfoWrapperPass>();
    // We need ScalarEvolution.
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
    Info.addPreserved<llvm::ScalarEvolutionWrapperPass>();
    // We donot preserve the scev infomation as we may change function body.
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
          this->StaticMemAccessCount++;
        //   this->DynamicMemAccessCount +=
        //       this->Trace->StaticToDynamicMap.at(Inst).size();
        }
      }
    }

    // Call the base runOnFunction to prepare it for replay.
    return ReplayTrace::runOnFunction(Function);
  }

  bool doFinalization(llvm::Module& Module) override {
    DEBUG(llvm::errs() << "StaticMemAccessCount: " << this->StaticMemAccessCount
                       << '\n');
    DEBUG(llvm::errs() << "DynamicMemAccessCount: "
                       << this->DynamicMemAccessCount << '\n');
    return false;
  }

 protected:
  //   void TransformTrace() override {}

  uint64_t StaticMemAccessCount;
  uint64_t StaticStreamCount;
  uint64_t StaticLengthDeterminableStreamCount;

  uint64_t DynamicMemAccessCount;
  uint64_t DynamicStreamCount;

  std::unordered_map<llvm::Instruction*, std::unordered_map<uint64_t, uint64_t>>
      StaticStreamInstiatedHistogram;
};

#undef DEBUG_TYPE
char StreamReplayTrace::ID = 0;
static llvm::RegisterPass<StreamReplayTrace> R(
    "stream-replay", "replay the llvm trace with stream inferface", false,
    false);