// This pass will ignore the detailed dynamic value, but collects
// some useful statistics from the trace.

#include "DataGraph.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "LocateAccelerableFunctions.h"
#include "Tracer.h"

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
  TraceStatisticPass()
      : llvm::FunctionPass(ID), DynamicInstCount("DynamicInstCount"),
        AccelerableDynamicInstCount("AccelerableDynamicInstCount") {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    // We require the loop information.
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
  }

  bool doInitialization(llvm::Module &Module) override {
    this->Module = &Module;

    // It is enough to collect some statistic in simple mode.
    this->Trace =
        new DataGraph(this->Module, DataGraph::DataGraphDetailLv::SIMPLE);

    // Simple sliding window method to read in the trace.
    uint64_t Count = 0;
    bool Ended = false;
    while (true) {
      if (this->DynamicInstCount.Val % 10000 == 0) {
        DEBUG(llvm::errs() << "Handled " << this->DynamicInstCount.Val << '\n');
      }

      if (!Ended) {
        auto NewDynamicInstIter = this->Trace->loadOneDynamicInst();
        if (NewDynamicInstIter != this->Trace->DynamicInstructionList.end()) {
          Count++;
        } else {
          Ended = true;
        }
      }

      if (Count == 0 && Ended) {
        // We are done.
        break;
      }

      // Generate the trace for gem5.
      // Maintain the window size of 10000?
      const uint64_t Window = 10000;
      if (Count > Window || Ended) {
        this->collectStatistics(this->Trace->DynamicInstructionList.front());
        Count--;
        this->Trace->commitOneDynamicInst();
      }
    }

    return false;
  }

  bool runOnFunction(llvm::Function &Function) override {
    auto &Info = getAnalysis<LocateAccelerableFunctions>();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        if (Info.isAccelerable(Function.getName())) {
          auto StaticInstCountIter = this->StaticInstCount.find(&*InstIter);
          if (StaticInstCountIter != this->StaticInstCount.end()) {
            this->AccelerableDynamicInstCount.Val +=
                StaticInstCountIter->second;
          }
        }
      }
    }

    return false;
  }

  void print(llvm::raw_ostream &O, const llvm::Module *M) const override {
    this->DynamicInstCount.print(O);
    this->AccelerableDynamicInstCount.print(O);
    for (const auto &CalleeName : this->ExternalCallCount) {
      O << "External call to " << CalleeName.first << ": " << CalleeName.second
        << '\n';
    }
  }

private:
  llvm::Module *Module;
  DataGraph *Trace;

  ScalarUIntVar DynamicInstCount;
  ScalarUIntVar AccelerableDynamicInstCount;

  std::unordered_map<llvm::Instruction *, uint64_t> StaticInstCount;

  // Count the external calls that are un traced.
  std::unordered_map<std::string, uint64_t> ExternalCallCount;

  void collectStatistics(DynamicInstruction *DynamicInst) {
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(StaticInst != nullptr &&
           "There should have a static instruction here.");

    this->DynamicInstCount.Val++;
    if (this->StaticInstCount.find(StaticInst) == this->StaticInstCount.end()) {
      this->StaticInstCount[StaticInst] = 1;
    } else {
      this->StaticInstCount[StaticInst]++;
    }

    if (auto StaticCall = llvm::dyn_cast<llvm::CallInst>(StaticInst)) {
      // DEBUG(llvm::errs() << "Get calling convention as "
      //                    << StaticCall->getCallingConv() << '\n');
      auto Callee = StaticCall->getCalledFunction();
      if (Callee == nullptr) {
        // This is indirect call. Ignore it.
      } else {
        if (Callee->isDeclaration()) {
          // This is external call
          std::string CalleeName = Callee->getName();
          if (this->ExternalCallCount.find(CalleeName) ==
              this->ExternalCallCount.end()) {
            this->ExternalCallCount.at(CalleeName)++;
          } else {
            this->ExternalCallCount.emplace(CalleeName, 1);
          }
        }
      }
    }
  }
};

} // namespace

#undef DEBUG_TYPE

char TraceStatisticPass::ID = 0;
static llvm::RegisterPass<TraceStatisticPass>
    X("trace-statistic-pass", "Trace statistic pass", false, true);
