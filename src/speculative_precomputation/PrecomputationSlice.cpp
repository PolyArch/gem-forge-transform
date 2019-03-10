#include "PrecomputationSlice.h"

#include <unordered_set>

PrecomputationSlice::PrecomputationSlice(llvm::LoadInst *_CriticalInst,
                                         DataGraph *_DG)
    : CriticalInst(_CriticalInst), DG(_DG) {
  this->initializeSlice();
}

void PrecomputationSlice::initializeSlice() {
  /**
   * Find all the instruction in the RIB.
   * Either we reaches the size of RIB or we encounter the inst twice.
   */
  assert(this->DG->DynamicInstructionList.back()->getStaticInstruction() ==
             this->CriticalInst &&
         "The last instruction should always be the critical instruction.");
  constexpr size_t RIBMaxSize = 512;
  size_t RIBSize = 0;

  std::unordered_set<DynamicInstruction::DynamicId> LiveInInstructions;
  LiveInInstructions.insert(this->DG->DynamicInstructionList.back()->getId());

  for (auto Iter = this->DG->DynamicInstructionList.rbegin(),
            End = this->DG->DynamicInstructionList.rend();
       Iter != End && RIBSize < RIBMaxSize; ++Iter, ++RIBSize) {
    auto DynamicInst = *Iter;
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(StaticInst != nullptr && "Failed to get staic instruction in RIB.");
    if (StaticInst == this->CriticalInst && RIBSize > 0) {
      // This is the second time we encounter the critical inst, break.
      break;
    }

    auto DynamicId = DynamicInst->getId();
    if (LiveInInstructions.count(DynamicId) == 0) {
      // This is not an alive instruction, ignore it.
      continue;
    }

    // Add to the slice.
    this->StaticSlice.emplace_front(StaticInst);
    this->DynamicSlice.emplace_front(DynamicInst);

    // Update the live instructions to the register dependence.
    LiveInInstructions.erase(DynamicId);
    const auto &RegDeps = this->DG->RegDeps.at(DynamicId);
    for (const auto &RegDep : RegDeps) {
      LiveInInstructions.insert(RegDep.second);
    }
  }
}