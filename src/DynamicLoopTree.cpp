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
      for (auto &Nest : this->NestLoopIters) {
        Nest.second->addInst(InstIter);
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