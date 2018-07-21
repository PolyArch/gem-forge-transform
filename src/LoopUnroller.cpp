#include "LoopUnroller.h"
#include "LoopUtils.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "LoopUnroller"

LoopUnroller::LoopUnroller(llvm::Loop *_Loop, llvm::ScalarEvolution *SE)
    : Loop(_Loop) {

  assert(this->Loop->empty() && "Can only unroll inner most loop for now.");

  for (auto BBIter = this->Loop->block_begin(), BBEnd = this->Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        llvm::InductionDescriptor ID;
        if (llvm::InductionDescriptor::isInductionPHI(PHINode, Loop, SE, ID)) {

          // // This is an induction variable. We try to find the updating
          // binary
          // // operation.
          // auto IVBinaryOp = this->findUpdatingBinaryOp(PHINode);

          this->InductionVars.emplace(PHINode, ID);
          this->IVToStartDynamicIdMap.emplace(Inst,
                                              DynamicInstruction::InvalidId);
        }
      }
    }
  }
}

void LoopUnroller::updateCurrentIter(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  if (StaticInst == nullptr) {
    // We assume non-llvm instruction is not the header.
    return;
  }
  // Make sure that we are always in the loop.
  assert(this->Loop->contains(StaticInst) &&
         "LoopUnroller falls outside of the loop.");
  if (LoopUtils::isStaticInstLoopHead(this->Loop, StaticInst)) {
    this->CurrentIter++;
  }
}

void LoopUnroller::updateIVToStartDynamicIdMap(
    llvm::Instruction *IV, DynamicInstruction::DynamicId Id) {
  assert(this->IVToStartDynamicIdMap.find(IV) !=
             this->IVToStartDynamicIdMap.end() &&
         "This is not an in-loop instruction.");
  DEBUG(llvm::errs() << "updateDepsDynamicIdMap for inst " << IV->getName()
                     << " " << IV->getOpcodeName() << " at iter "
                     << this->CurrentIter << '\n');
  if (this->CurrentIter != 0) {
    // Only care about the first iteration.
    return;
  }
  auto &StartId = this->IVToStartDynamicIdMap.at(IV);
  if (StartId != DynamicInstruction::InvalidId) {
    // We have seen this dependence in the current iteration.
    // Make sure they are still the same.
    assert(StartId == Id && "Mismatch dynamic ids for induction variable.");
  } else {
    // This is the first time we have seen this dependence in the first
    // iteration.
    StartId = Id;
  }
}

void LoopUnroller::fixAndUpdateIVRegDeps(DynamicInstruction *DynamicInst,
                                         DataGraph *DG) {

  DEBUG(llvm::errs() << "fixAndUpdateIVRegDeps for inst "
                     << DynamicInst->getOpName() << " at iter "
                     << this->CurrentIter << '\n');
  auto &RegDeps = DG->RegDeps.at(DynamicInst->Id);
  for (auto RegDepIter = RegDeps.begin(), RegDepEnd = RegDeps.end();
       RegDepIter != RegDepEnd;) {
    auto StaticOperand = RegDepIter->first;
    if (StaticOperand == nullptr) {
      // Ingore those non-llvm dependence.
      ++RegDepIter;
      continue;
    }
    DEBUG(llvm::errs() << "Reg dependent on inst " << StaticOperand->getName()
                       << '\n');
    if (this->InductionVars.find(StaticOperand) != this->InductionVars.end()) {
      // This is an induction variable.
      assert(this->IVToStartDynamicIdMap.find(StaticOperand) !=
                 this->IVToStartDynamicIdMap.end() &&
             "Missing dynamic ids.");
      this->updateIVToStartDynamicIdMap(StaticOperand, RegDepIter->second);
      // Update the dependence to the same as first iteration.
      auto StartId = this->IVToStartDynamicIdMap.at(StaticOperand);
      DEBUG(llvm::errs() << "IV " << StaticOperand->getName() << " start id "
                         << StartId << '\n');
      if (StartId != DynamicInstruction::InvalidId) {
        RegDepIter->second = StartId;
      } else {
        // The initial value of the induction variable does not come from an
        // instruction, there is no dependence.
        DEBUG(llvm::errs() << "Erase dependence on IV "
                           << StaticOperand->getName() << '\n');
        RegDepIter = RegDeps.erase(RegDepIter);
        continue;
      }
    }
    // Update the iterator to the next one.
    ++RegDepIter;
  }
}

void LoopUnroller::fixAndUpdateCtrDeps(DynamicInstruction *DynamicInst,
                                       DataGraph *DG) {
  std::list<DynamicId> RemovedCtrDeps;
  std::list<DynamicId> AddedCtrDeps;
  auto &CtrDeps = DG->CtrDeps.at(DynamicInst->Id);
  for (auto CtrDepId : CtrDeps) {
    auto CtrDepDynamicInst = DG->getAliveDynamicInst(CtrDepId);
    if (CtrDepDynamicInst == nullptr) {
      // We know that if the dependent instruction is dead, it must not be
      // within our unroll window.
      this->OutsideCtrDeps.insert(CtrDepId);
      continue;
    }
    auto CtrDepStaticInst = CtrDepDynamicInst->getStaticInstruction();
    assert(CtrDepStaticInst != nullptr &&
           "Control dependent on non-llvm instruction.");
    if (!this->Loop->contains(CtrDepStaticInst)) {
      // Keep outside loop dependence.
      this->OutsideCtrDeps.insert(CtrDepId);
      continue;
    }
    // After this point this is an inside control dependence.
    auto DepIterationIterator = this->DynamicIdToIterationMap.find(CtrDepId);
    if (DepIterationIterator == this->DynamicIdToIterationMap.end()) {
      // This is simply a control dependence falling outside of our unroll
      // window.
      this->OutsideCtrDeps.insert(CtrDepId);
      continue;
    }
    int DepIter = this->DynamicIdToIterationMap.at(CtrDepId);
    if (DepIter != this->CurrentIter) {
      // Inter-iter control dependence.
      // Make it dependent all the outside deps.
      RemovedCtrDeps.push_back(CtrDepId);
      for (auto OutCtrDep : OutsideCtrDeps) {
        AddedCtrDeps.push_back(OutCtrDep);
      }
    }
  }
  for (auto Id : RemovedCtrDeps) {
    CtrDeps.erase(Id);
  }
  for (auto Id : AddedCtrDeps) {
    CtrDeps.insert(Id);
  }
}

void LoopUnroller::unroll(DataGraph *DG, DataGraph::DynamicInstIter Begin,
                          DataGraph::DynamicInstIter End) {

  assert(this->canUnroll() && "This loop can not be unrolled.");

  this->CurrentIter = -1;
  for (auto &Entry : this->IVToStartDynamicIdMap) {
    Entry.second = DynamicInstruction::InvalidId;
  }
  this->DynamicIdToIterationMap.clear();
  this->OutsideCtrDeps.clear();

  for (auto InstIter = Begin; InstIter != End; ++InstIter) {
    auto DynamicInst = *InstIter;
    // Update the current iter.
    this->updateCurrentIter(DynamicInst);
    assert(this->CurrentIter >= 0 &&
           "The first dynamic instruction is not loop head.");
    // Fix the register dependence.
    this->fixAndUpdateIVRegDeps(DynamicInst, DG);
    // Fix the control dependence.
    this->fixAndUpdateCtrDeps(DynamicInst, DG);
    // Update our data structure.
    this->DynamicIdToIterationMap.emplace(DynamicInst->Id, this->CurrentIter);
  }
}

CachedLoopUnroller::~CachedLoopUnroller() {
  for (auto &Entry : this->CachedLU) {
    delete Entry.second;
  }
  this->CachedLU.clear();
}

LoopUnroller *CachedLoopUnroller::getUnroller(llvm::Loop *Loop,
                                              llvm::ScalarEvolution *SE) {
  auto LIter = this->CachedLU.find(Loop);
  if (LIter == this->CachedLU.end()) {
    LIter = this->CachedLU.emplace(Loop, new LoopUnroller(Loop, SE)).first;
  }
  return LIter->second;
}