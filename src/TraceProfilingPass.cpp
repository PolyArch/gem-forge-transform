/**
 * A key problem is to determine whether we have traced
 * sufficient and representative blocks.
 *
 * This pass will collect and print the distrubtion of
 * all traces and the overall profiling result, to help
 * the user to determine if we should trace more.
 *
 * Unlike other passes, this pass will process all the
 * traces.
 */

#include "ProfileParser.h"
#include "Replay.h"
#include "Utils.h"

#include <fstream>
#include <sstream>

#define DEBUG_TYPE "TraceProfilingPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

namespace {

class TraceProfile {
public:
  TraceProfile(llvm::Module *_Module) : TotalDynamicInstCount(0) {}

  void addInst(llvm::Instruction *Inst, uint64_t Value = 1);

private:
  uint64_t TotalDynamicInstCount;
  std::unordered_map<llvm::BasicBlock *, uint64_t> BBDynamicInstCount;
  std::unordered_map<llvm::Function *, uint64_t> FuncDynamicInstCount;
  std::unordered_map<llvm::Loop *, uint64_t> LoopDynamicInstCount;
};

class TraceProfilingPass : public ReplayTrace {
public:
  static char ID;
  TraceProfilingPass(char _ID = ID);

protected:
  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;
  void dumpStats(std::ostream &O) override;
  void transform() override;

private:
  std::unordered_map<std::string, std::unique_ptr<TraceProfile>> TraceFileNames;
  std::unique_ptr<TraceProfile> AllTracesProfile;

  ProfileParser Profile;
};

TraceProfilingPass::TraceProfilingPass(char _ID) : ReplayTrace(_ID) {}

bool TraceProfilingPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);

  this->AllTracesProfile = std::make_unique<TraceProfile>(this->Module);

  /**
   * Collect all the traces.
   */
  auto Fields = Utils::splitByChar(TraceFileName, '.');
  auto TraceId = std::stoi(Fields[Fields.size() - 2]);
  while (true) {
    // Reconstruct the trace file name.
    std::stringstream S;
    for (auto I = 0; I < Fields.size(); ++I) {
      if (I != 0) {
        S << '.';
      }
      if (I + 2 == Fields.size()) {
        S << TraceId;
      } else {
        S << Fields[I];
      }
    }
    // Check if the file name exists.
    auto FN = S.str();
    std::ifstream F(FN.c_str());
    if (!F.good()) {
      break;
    } else {
      TraceId++;
      this->TraceFileNames.emplace(FN, new TraceProfile(this->Module));
    }
  }

  assert(!this->TraceFileNames.empty() &&
         "Failed to find valid trace file name.");

  return Ret;
}

bool TraceProfilingPass::finalize(llvm::Module &Module) {
  this->TraceFileNames.clear();
  return ReplayTrace::finalize(Module);
}

void TraceProfilingPass::dumpStats(std::ostream &O) { return; }

void TraceProfilingPass::transform() {
  for (const auto &FNIter : this->TraceFileNames) {
    const auto &FN = FNIter.first;
    auto &TP = FNIter.second;
    TraceFileName.setValue(FN, false);
    delete this->Trace;
    this->Trace = new DataGraph(this->Module, this->CachedLI, this->CachedPDF,
                                this->CachedLU, this->DGDetailLevel);

    while (true) {
      auto NewInstIter = this->Trace->loadOneDynamicInst();
      if (NewInstIter != this->Trace->DynamicInstructionList.end()) {
        auto StaticInst = (*NewInstIter)->getStaticInstruction();
        TP->addInst(StaticInst);
        this->AllTracesProfile->addInst(StaticInst);
      } else {
        break;
      }
    }
  }
}

void TraceProfile::addInst(llvm::Instruction *Inst, uint64_t Value) {
  this->TotalDynamicInstCount += Value;
  this->BBDynamicInstCount.emplace(Inst->getParent(), 0).first->second += Value;
  this->FuncDynamicInstCount.emplace(Inst->getFunction(), 0).first->second +=
      Value;
}

} // namespace