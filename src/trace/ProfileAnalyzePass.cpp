#include "ProfileAnalyzePass.h"

#include <fstream>
#include <iomanip>

#define DEBUG_TYPE "GemForgeBasePass"

ProfileAnalyzePass::ProfileAnalyzePass() : GemForgeBasePass(ID) {}
ProfileAnalyzePass::~ProfileAnalyzePass() {}

bool ProfileAnalyzePass::runOnModule(llvm::Module &Module) {
  this->initialize(Module);

  this->Profile = std::make_unique<ProfileParser>();

  this->TotalInsts = 0;
  for (const auto &FuncEntry : this->Profile->Profile.funcs()) {
    const auto &FuncName = FuncEntry.first;
    auto Func = this->Module->getFunction(FuncName);
    assert(Func && "Failed to get function.");
    this->analyzeRegionInFunc(Func);
  }

  this->sortOuterMostLoops();
  this->selectRegionSimpoints();

  this->dumpTo();

  this->finalize(Module);
  return false;
}

void ProfileAnalyzePass::analyzeRegionInFunc(llvm::Function *Func) {
  auto LI = this->CachedLI->getLoopInfo(Func);
  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    auto BB = &*BBIter;
    auto InstCount = this->Profile->countBasicBlock(BB);
    this->TotalInsts += InstCount;
    if (InstCount == 0) {
      continue;
    }
    auto Loop = LI->getLoopFor(BB);
    while (Loop) {
      this->LoopInstCountMap.emplace(Loop, 0).first->second += InstCount;
      Loop = Loop->getParentLoop();
    }
  }
}

uint64_t ProfileAnalyzePass::getLoopInstCount(llvm::Loop *Loop) const {
  auto Iter = this->LoopInstCountMap.find(Loop);
  if (Iter == this->LoopInstCountMap.end()) {
    return 0;
  }
  return Iter->second;
}

void ProfileAnalyzePass::sortOuterMostLoops() {
  for (const auto &LoopEntry : this->LoopInstCountMap) {
    auto &Loop = LoopEntry.first;
    if (!Loop->getParentLoop()) {
      this->SortedOuterMostLoops.emplace_back(Loop);
    }
  }
  std::sort(this->SortedOuterMostLoops.begin(),
            this->SortedOuterMostLoops.end(),
            [this](llvm::Loop *A, llvm::Loop *B) -> bool {
              return this->getLoopInstCount(A) > this->getLoopInstCount(B);
            });
}

void ProfileAnalyzePass::selectRegionSimpoints() {
  float Coverage = 0.0f;
  for (auto &OuterMostLoop : this->SortedOuterMostLoops) {
    auto OuterMostLoopDepth = OuterMostLoop->getLoopDepth();

    // Maybe stop if we have 90% coverage.
    if (Coverage > 0.9f) {
      break;
    }

    std::vector<llvm::Loop *> LoopStack;
    LoopStack.push_back(OuterMostLoop);
    while (!LoopStack.empty()) {
      auto Loop = LoopStack.back();
      LoopStack.pop_back();
      if (LoopUtils::isLoopContinuous(Loop)) {
        auto Count = this->getLoopInstCount(Loop);
        Coverage +=
            static_cast<float>(Count) / static_cast<float>(this->TotalInsts);
        this->SortedRegionSimpoints.push_back(Loop);
      } else {
        // Push subloops.
        for (auto &SubLoop : Loop->getSubLoops()) {
          LoopStack.push_back(SubLoop);
        }
      }
    }
  }

  std::sort(this->SortedRegionSimpoints.begin(),
            this->SortedRegionSimpoints.end(),
            [this](llvm::Loop *A, llvm::Loop *B) -> bool {
              return this->getLoopInstCount(A) > this->getLoopInstCount(B);
            });
}

void ProfileAnalyzePass::dumpTo() const {

  std::ofstream O(ProfileFolder.getValue() + "/region.profile.txt");
  // Dump selected region simpoints.
  for (auto &RegionSimpoint : this->SortedRegionSimpoints) {
    this->dumpLoopTo(RegionSimpoint, O);
  }
  O << "===============================================\n";
  // Dump all loops.
  for (auto &OuterMostLoop : this->SortedOuterMostLoops) {
    this->dumpLoopTo(OuterMostLoop, O);
  }
  O.close();

  // Dump selected region simpoints.
  {
    std::ofstream O(ProfileFolder.getValue() + "/region.simpoints.txt");
    for (auto &RegionSimpoint : this->SortedRegionSimpoints) {
      this->dumpLoopTo(RegionSimpoint, O, false);
    }
    O.close();
  }
}

void ProfileAnalyzePass::dumpLoopTo(llvm::Loop *OuterMostLoop, std::ostream &O,
                                    bool Expand) const {
  for (auto &Loop : OuterMostLoop->getLoopsInPreorder()) {
    auto Count = 0;
    /**
     * It is possible that this loop is not executed, maybe due to branch
     * condition.
     */
    {
      auto Iter = this->LoopInstCountMap.find(Loop);
      if (Iter != this->LoopInstCountMap.end()) {
        Count = Iter->second;
      }
    }
    O << std::fixed << std::setprecision(3)
      << (static_cast<float>(Count) / static_cast<float>(this->TotalInsts))
      << ' ';
    O << std::setw(12) << Count << ' ' << LoopUtils::isLoopContinuous(Loop)
      << ' ';
    // Dump indent.
    auto Depth = Loop->getLoopDepth();
    for (auto D = 0; D < Depth; ++D) {
      O << "  ";
    }
    O << LoopUtils::getLoopId(Loop) << ' '
      << Loop->getHeader()->getParent()->getName().str() << ' '
      << Loop->getHeader()->getName().str() << '\n';
    // If not expand, only dump the outer most loop.
    if (!Expand) {
      break;
    }
  }
}

#undef DEBUG_TYPE

char ProfileAnalyzePass::ID = 0;
static llvm::RegisterPass<ProfileAnalyzePass>
    X("profile-analyze-pass", "Analyze the profile pass", false, false);