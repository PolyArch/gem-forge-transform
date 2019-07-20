// This pass will ignore the detailed dynamic value, but collects
// some useful statistics from the trace.

#include "DataGraph.h"
#include "LocateAccelerableFunctions.h"
#include "LoopUtils.h"
#include "trace/ProfileParser.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <map>
#include <set>
#include <unordered_set>

#define DEBUG_TYPE "TraceStatisticPass"
namespace {

class ScalarUIntVar {
public:
  ScalarUIntVar(const std::string &_name) : Val(0), name(_name) {}
  uint64_t Val;
  std::string name;
  void print(llvm::raw_ostream &O) const {
    O << this->name << ": " << this->Val << '\n';
  }
};

// This is a pass to instrument a function and trace it.
class TraceStatisticPass : public llvm::FunctionPass {
public:
  static char ID;

#define INIT_VAR(var) var(#var)
  TraceStatisticPass()
      : llvm::FunctionPass(ID), INIT_VAR(DynInstCount),
        INIT_VAR(DynMemInstCount), INIT_VAR(TracedDynInstCount),
        INIT_VAR(AddRecLoadCount), INIT_VAR(AddRecStoreCount) {}
#undef INIT_VAR

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    // We require the loop information.
    // Info.addRequired<LocateAccelerableFunctions>();
    // Info.addPreserved<LocateAccelerableFunctions>();
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
    Info.addPreserved<llvm::ScalarEvolutionWrapperPass>();
  }

  bool doInitialization(llvm::Module &Module) override {
    this->Module = &Module;

    // // It is enough to collect some statistic in simple mode.
    // this->Trace =
    //     new DataGraph(this->Module, this->CachedLI, this->CachedPDf,
    //     this->CachedLU, DataGraph::DataGraphDetailLv::SIMPLE);

    // // Simple sliding window method to read in the trace.
    // uint64_t Count = 0;
    // bool Ended = false;
    // while (true) {

    //   if (!Ended) {
    //     auto NewDynamicInstIter = this->Trace->loadOneDynamicInst();
    //     if (NewDynamicInstIter != this->Trace->DynamicInstructionList.end())
    //     {
    //       Count++;
    //     } else {
    //       Ended = true;
    //     }
    //   }

    //   if (Count == 0 && Ended) {
    //     // We are done.
    //     break;
    //   }

    //   // Generate the trace for gem5.
    //   // Maintain the window size of 10000?
    //   const uint64_t Window = 10000;
    //   if (Count > Window || Ended) {
    //     this->collectStatistics(this->Trace->DynamicInstructionList.front());
    //     Count--;
    //     this->Trace->commitOneDynamicInst();
    //   }
    // }

    return false;
  }

  bool doFinalization(llvm::Module &Module) override {
    this->DynInstCount.print(llvm::errs());
    this->DynMemInstCount.print(llvm::errs());
    this->TracedDynInstCount.print(llvm::errs());
    this->AddRecLoadCount.print(llvm::errs());
    this->AddRecStoreCount.print(llvm::errs());
    return false;
  }

  bool runOnFunction(llvm::Function &Function) override {

    llvm::ScalarEvolution &SE =
        this->getAnalysis<llvm::ScalarEvolutionWrapperPass>().getSE();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = &*BBIter;
      auto BBCount = this->Profile.countBasicBlock(BB);
      if (BBCount == 0) {
        // Ignore those basic blocks missing from profiling.
        continue;
      }
      auto PHIs = BB->phis();
      this->DynInstCount.Val +=
          (BB->size() - std::distance(PHIs.begin(), PHIs.end())) * BBCount;
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        if (!llvm::isa<llvm::LoadInst>(Inst) &&
            !llvm::isa<llvm::StoreInst>(Inst)) {
          continue;
        }
        bool IsLoad = llvm::isa<llvm::LoadInst>(Inst);
        // This is a memory access.
        this->DynMemInstCount.Val += BBCount;
        llvm::Value *Addr = nullptr;
        if (IsLoad) {
          Addr = Inst->getOperand(0);
        } else {
          Addr = Inst->getOperand(1);
        }
        // Check if this is a stream.
        const llvm::SCEV *SCEV = SE.getSCEV(Addr);

        if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(SCEV)) {
          // This is a stream.
          DEBUG(llvm::errs()
                << "Stream " << LoopUtils::formatLLVMInst(Inst) << '\n');
          if (IsLoad) {
            this->AddRecLoadCount.Val += BBCount;
          } else {
            this->AddRecStoreCount.Val += BBCount;
          }
          // if (AddRecSCEV->isAffine()) {

          // }
          // if (AddRecSCEV->isQuadratic()) {
          //   this->StaticQuadricStreamCount++;
          // }
        }
      }
    }

    return false;
  }

  void print(llvm::raw_ostream &O, const llvm::Module *M) const override {}

private:
  llvm::Module *Module;
  DataGraph *Trace;

  ScalarUIntVar DynInstCount;
  ScalarUIntVar DynMemInstCount;
  ScalarUIntVar TracedDynInstCount;

  ScalarUIntVar AddRecLoadCount;
  ScalarUIntVar AddRecStoreCount;

  void collectStatistics(DynamicInstruction *DynamicInst) {
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(StaticInst != nullptr &&
           "There should have a static instruction here.");
    this->TracedDynInstCount.Val++;
  }

  ProfileParser Profile;
};

} // namespace

#undef DEBUG_TYPE

char TraceStatisticPass::ID = 0;
static llvm::RegisterPass<TraceStatisticPass>
    X("trace-statistic-pass", "Trace statistic pass", false, true);
