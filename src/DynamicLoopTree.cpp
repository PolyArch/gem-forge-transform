#include "DynamicLoopTree.h"

DynamicLoopIteration::DynamicLoopIteration(llvm::Loop *_Loop,
                                           DynamicInstIter _End)
    : Loop(_Loop), Start(_End), End(_End), NextIter(nullptr), Status(EMPTY) {

  // Initialize the nested loop.
  for (auto NestIter = this->Loop->begin(), NestEnd = this->Loop->end();
       NestIter != NestEnd; ++NestIter) {
    this->NestLoopIters.emplace(*NestIter,
                                new DynamicLoopIteration(*NestIter, _End));
  }
}

DynamicLoopIteration::~DynamicLoopIteration() {
  if (this->NextIter != nullptr) {
    delete this->NextIter;
    this->NextIter = nullptr;
  }
  for (auto &Nest : this->NestLoopIters) {
    if (Nest.second != nullptr) {
      delete Nest.second;
      Nest.second = nullptr;
    }
  }
}

void DynamicLoopIteration::addInst(DynamicInstIter InstIter) {

  auto DynamicInst = *InstIter;
  auto StaticInst = DynamicInst->getStaticInstruction();
  bool IsInLoop = this->Loop->contains(StaticInst);
  bool IsLoopHead = LoopUtils::isStaticInstLoopHead(this->Loop, StaticInst);

  switch (this->Status) {
  case EMPTY: {
    if (IsLoopHead) {
      // We just entered the loop.
      this->Start = InstIter;
      this->Status = BUFFERING;
      assert(this->StaticToDynamicMap.empty() &&
             "StaticToDynamicMap should be empty in EMPTY state.");
      this->StaticToDynamicMap.emplace(StaticInst, DynamicInst);
    }
    break;
  }
  case BUFFERING: {
    if (!IsInLoop) {
      // We are out of the loop.
      this->Status = COMPLETED;
      this->End = InstIter;
    } else if (IsLoopHead) {
      // We reached the next iteration.
      this->End = InstIter;
      this->Status = COMPLETED;
      // Allocate the next iteration.
      this->NextIter = new DynamicLoopIteration(this->Loop, this->End);
      this->NextIter->addInst(InstIter);
    } else {
      // This instruction is in the loop.
      // Add to all the nest loops.
      bool IsInNestedLoop = false;
      for (auto &Nest : this->NestLoopIters) {
        Nest.second->addInst(InstIter);
        IsInNestedLoop =
            IsInNestedLoop || Nest.second->getLoop()->contains(StaticInst);
      }
      if (!IsInNestedLoop) {
        // If not in nested loop, add to my StaticToDynamicMap.
        auto Emplaced =
            this->StaticToDynamicMap.emplace(StaticInst, DynamicInst).second;
        assert(Emplaced && "Multiple dynamic instructions in one iteration?");
      }
    }
    break;
  }
  case COMPLETED: {
    if (this->NextIter != nullptr) {
      this->NextIter->addInst(InstIter);
    }
    break;
  }
  default: {
    llvm_unreachable("DynamicLoopIteration: Invalid status.");
    break;
  }
  }
}

void DynamicLoopIteration::addInstEnd(DynamicInstIter InstIter) {

  switch (this->Status) {
  case EMPTY: {
    break;
  }
  case BUFFERING: {
    this->Status = COMPLETED;
    this->End = InstIter;
    for (auto &Nest : this->NestLoopIters) {
      Nest.second->addInstEnd(InstIter);
    }
    break;
  }
  case COMPLETED: {
    if (this->NextIter != nullptr) {
      this->NextIter->addInstEnd(InstIter);
    }
    break;
  }
  default: {
    llvm_unreachable("DynamicLoopIteration: Invalid status.");
    break;
  }
  }
}

size_t DynamicLoopIteration::countIter() const {
  if (this->Status == COMPLETED) {
    if (this->NextIter != nullptr) {
      return 1 + this->NextIter->countIter();
    } else {
      return 1;
    }
  } else {
    return 0;
  }
}

DynamicLoopIteration *DynamicLoopIteration::moveTailIter() {
  size_t Iter = this->countIter();
  assert(Iter > 0 && "moveTailIter called for 0 iteration.");
  if (Iter == 1) {
    // I am the last complete iteration.
    auto TailIter = this->NextIter;
    // Clear my ownership.
    this->NextIter = nullptr;
    return TailIter;
  } else {
    return this->NextIter->moveTailIter();
  }
}

DynamicLoopIteration *DynamicLoopIteration::getChildIter(llvm::Loop *Child) {
  assert(Child->getParentLoop() == this->Loop &&
         "Invalid child for this->Loop");
  return this->NestLoopIters.at(Child);
}

DynamicInstruction *
DynamicLoopIteration::getDynamicInst(llvm::Instruction *StaticInst) const {
  assert(
      this->Loop->contains(StaticInst) &&
      "Try getting dynamic instruction for static instruction not within this "
      "loop.");

  bool IsInNestedLoop = false;
  for (auto &Nest : this->NestLoopIters) {
    IsInNestedLoop =
        IsInNestedLoop || Nest.second->getLoop()->contains(StaticInst);
  }

  assert(!IsInNestedLoop && "Try getting dynamic instruction for static "
                            "instruction within nested loops.");

  auto Iter = this->StaticToDynamicMap.find(StaticInst);
  if (Iter == this->StaticToDynamicMap.end()) {
    return nullptr;
  }
  return Iter->second;
}