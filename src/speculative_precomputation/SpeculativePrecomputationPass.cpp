#include "SpeculativePrecomputationPass.h"

llvm::cl::opt<std::string>
    LoadProfileFileName("speculative-precomputation-load-profile-file",
                        llvm::cl::desc("Load profile file."));

#define DEBUG_TYPE "SpeculativePrecomputationPass"

bool SpeculativePrecomputationPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);

  this->InstManagerMap.clear();

  return Ret;
}

bool SpeculativePrecomputationPass::finalize(llvm::Module &Module) {
  this->InstManagerMap.clear();
  return ReplayTrace::finalize(Module);
}

void SpeculativePrecomputationPass::transform() {
  // First initialize all the critical loads.
  this->initializeCriticalLoadManagers();
}

void SpeculativePrecomputationPass::dumpStats(std::ostream &O) {}

void SpeculativePrecomputationPass::initializeCriticalLoadManagers() {
  assert(LoadProfileFileName.getNumOccurrences() > 0 &&
         "Please specify the load profile.");
  std::ifstream LoadProfileFStream(LoadProfileFileName.getValue());
  assert(LoadProfileFStream.is_open() && "Failed to open load profile file.");

  const auto &InstUIDMap = this->Trace->getInstUIDMap();

  InstructionUIDMap::InstructionUID InstUID;
  float AverageLatency;
  while (LoadProfileFStream >> InstUID >> AverageLatency) {
    auto Inst = InstUIDMap.getInst(InstUID);
    auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst);
    assert(LoadInst != nullptr && "Non-Load inst in load profile.");
    if (AverageLatency > 40) {
      auto Inserted =
          this->InstManagerMap
              .emplace(std::piecewise_construct,
                       std::forward_as_tuple(LoadInst), std::forward_as_tuple())
              .second;
      assert(Inserted && "Failed to insert the critical load manager.");
    }
  }
}

#undef DEBUG_TYPE

char SpeculativePrecomputationPass::ID = 0;
static llvm::RegisterPass<SpeculativePrecomputationPass>
    X("speculative-precomputation-pass",
      "speculative precomputation transform pass", false, false);