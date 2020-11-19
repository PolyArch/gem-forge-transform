#include "SpeculativePrecomputationPass.h"

#include "Utils.h"

llvm::cl::opt<std::string>
    LoadProfileFileName("speculative-precomputation-load-profile-file",
                        llvm::cl::desc("Load profile file."));

#define DEBUG_TYPE "SpeculativePrecomputationPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

bool SpeculativePrecomputationPass::initialize(llvm::Module &Module) {
  bool Ret = ReplayTrace::initialize(Module);

  this->InstManagerMap.clear();
  // First initialize all the critical loads.
  this->initializeCriticalLoadManagers();

  return Ret;
}

bool SpeculativePrecomputationPass::finalize(llvm::Module &Module) {
  for (auto &InstManager : this->InstManagerMap) {
    InstManager.second.dump();
  }
  this->InstManagerMap.clear();
  return ReplayTrace::finalize(Module);
}

void SpeculativePrecomputationPass::transform() {
  LLVM_DEBUG(llvm::dbgs() << "SpeculativePrecomputation: start.\n");

  size_t InstCount = 0;
  while (true) {
    auto NewDynInstIter = this->Trace->loadOneDynamicInst();
    llvm::Instruction *NewStaticInst = nullptr;

    if (NewDynInstIter != this->Trace->DynamicInstructionList.end()) {
      NewStaticInst = (*NewDynInstIter)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      // Handle the cache warmer.
      if (Utils::isMemAccessInst(NewStaticInst)) {
        this->CacheWarmerPtr->addAccess(*NewDynInstIter,
                                        this->Trace->DataLayout);
      }
      InstCount++;
    } else {
      // Commit any remaining buffered instructions when hitting the end.
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                    this->Trace);
        this->Trace->commitOneDynamicInst();
      }
      break;
    }

    // Try to maintain the size of the buffer the same as retired instruction
    // buffer (RIB), which is 512.
    constexpr size_t RIB_SIZE = 512;
    while (this->Trace->DynamicInstructionList.size() > RIB_SIZE) {
      this->Serializer->serialize(this->Trace->DynamicInstructionList.front(),
                                  this->Trace);
      this->Trace->commitOneDynamicInst();
    }

    // Check if we found a critical load.
    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(NewStaticInst)) {
      auto InstManagerIter = this->InstManagerMap.find(LoadInst);
      if (InstManagerIter != this->InstManagerMap.end()) {
        auto &Manager = InstManagerIter->second;
        Manager.hit(this->Trace);
      }
    }

    // Check if we need to clear the slices periodically.
    constexpr size_t SLICE_CLEAR_PERIOD = 128000;
    if (InstCount % SLICE_CLEAR_PERIOD == 0) {
      for (auto &InstManager : this->InstManagerMap) {
        InstManager.second.clear();
      }
    }
  }

  LLVM_DEBUG(llvm::dbgs() << "SpeculativePrecomputation: done.\n");
}

void SpeculativePrecomputationPass::dumpStats(std::ostream &O) {}

void SpeculativePrecomputationPass::initializeCriticalLoadManagers() {
  assert(LoadProfileFileName.getNumOccurrences() > 0 &&
         "Please specify the load profile.");
  std::ifstream LoadProfileFStream(LoadProfileFileName.getValue());
  assert(LoadProfileFStream.is_open() && "Failed to open load profile file.");

  const auto &InstUIDMap = Utils::getInstUIDMap();

  InstructionUIDMap::InstructionUID InstUID;
  float AverageLatency;
  while (LoadProfileFStream >> InstUID >> AverageLatency) {
    auto Inst = InstUIDMap.getInst(InstUID);
    auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst);
    assert(LoadInst != nullptr && "Non-Load inst in load profile.");

    if (AverageLatency > 40) {
      auto Inserted =
          this->InstManagerMap
              .emplace(
                  std::piecewise_construct, std::forward_as_tuple(LoadInst),
                  std::forward_as_tuple(LoadInst, this->OutputExtraFolderPath))
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