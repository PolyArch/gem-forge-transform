#include "PrecomputationSlice.h"

#include <unordered_map>
#include <unordered_set>

PrecomputationSlice::PrecomputationSlice(llvm::LoadInst *_CriticalInst,
                                         DataGraph *_DG)
    : CriticalInst(_CriticalInst), DG(_DG) {
  this->initializeSlice();
}

PrecomputationSlice::~PrecomputationSlice() {
  // Release the dynamic template.
  for (auto &SliceInst : this->SliceTemplate) {
    delete SliceInst;
    SliceInst = nullptr;
  }
  this->SliceTemplate.clear();
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

  std::unordered_map<DynamicInstruction::DynamicId,
                     std::unordered_set<SliceInst *>>
      LiveInInstructions;
  LiveInInstructions.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(this->DG->DynamicInstructionList.back()->getId()),
      std::forward_as_tuple());

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
    this->SliceTemplate.emplace_front(
        new SliceInst(PrecomputationSlice::copyDynamicLLVMInst(DynamicInst)));

    // Update the user dependence.
    for (auto &LiveInInstUser : LiveInInstructions.at(DynamicId)) {
      LiveInInstUser->BaseSliceInsts.insert(this->SliceTemplate.front());
    }

    // Update the live instructions to the register dependence.
    LiveInInstructions.erase(DynamicId);
    const auto &RegDeps = this->DG->RegDeps.at(DynamicId);
    for (const auto &RegDep : RegDeps) {
      auto DepId = RegDep.second;
      LiveInInstructions
          .emplace(std::piecewise_construct, std::forward_as_tuple(DepId),
                   std::forward_as_tuple())
          .first->second.insert(this->SliceTemplate.front());
    }
  }
}

bool PrecomputationSlice::isSame(const PrecomputationSlice &Other) const {
  assert(this->CriticalInst == Other.CriticalInst &&
         "Should only compare slices with the same critical inst.");
  if (this->SliceTemplate.size() != Other.SliceTemplate.size()) {
    return false;
  }
  for (auto AIter = this->SliceTemplate.begin(),
            AEnd = this->SliceTemplate.end(),
            BIter = Other.SliceTemplate.begin(),
            BEnd = Other.SliceTemplate.end();
       AIter != AEnd && BIter != BEnd; ++AIter, ++BIter) {
    if ((*AIter)->DynamicInst->getStaticInstruction() !=
        (*BIter)->DynamicInst->getStaticInstruction()) {
      return false;
    }
  }
  return true;
}

void PrecomputationSlice::generateSlice(TDGSerializer *Serializer,
                                        bool IsReal) const {
  auto TemplateIter = this->SliceTemplate.begin();
  auto TemplateEnd = this->SliceTemplate.end();

  std::unordered_map<SliceInst *, DynamicInstruction::DynamicId>
      SliceInstDynamicIdMap;

  while (TemplateIter != TemplateEnd) {
    /**
     * Create the dynamic instruction.
     */
    auto SliceInst = *TemplateIter;
    LLVMDynamicInstruction *SliceDynamicInst =
        PrecomputationSlice::copyDynamicLLVMInst(SliceInst->DynamicInst);
    auto SliceDynamicId = SliceDynamicInst->getId();

    // Insert into the DG and set the dependence.
    this->DG->insertDynamicInst(this->DG->DynamicInstructionList.begin(),
                                SliceDynamicInst);
    // Only register dependence.
    auto &RegDeps = this->DG->RegDeps.at(SliceDynamicId);
    for (auto &BaseSliceInst : SliceInst->BaseSliceInsts) {
      assert(SliceInstDynamicIdMap.count(BaseSliceInst) > 0 &&
             "Failed to find the base slice instructions.");
      RegDeps.emplace_back(nullptr, SliceInstDynamicIdMap.at(BaseSliceInst));
    }

    // Serialize the inst.
    Serializer->serialize(SliceDynamicInst, this->DG);

    // Remove the inst from DG.
    this->DG->commitOneDynamicInst();

    // Add the new slice inst to SliceInstDynamicIdMap.
    SliceInstDynamicIdMap.emplace(SliceInst, SliceDynamicId);

    // Advance the iterator.
    ++TemplateIter;
  }
}

LLVMDynamicInstruction *PrecomputationSlice::copyDynamicLLVMInst(
    DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Only copy llvm dynamic instruction.");

  // Copy the dynamic arguments and results.
  std::vector<DynamicValue *> DynamicOperands;
  for (auto &Operand : DynamicInst->DynamicOperands) {
    DynamicOperands.push_back(new DynamicValue(*Operand));
  }
  DynamicValue *DynamicResult = nullptr;
  if (DynamicInst->DynamicResult) {
    DynamicResult = new DynamicValue(*DynamicInst->DynamicResult);
  }

  return new LLVMDynamicInstruction(StaticInst, DynamicResult,
                                    std::move(DynamicOperands));
}