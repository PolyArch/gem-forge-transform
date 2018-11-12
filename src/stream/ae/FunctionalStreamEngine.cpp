#include "FunctionalStreamEngine.h"

#define DEBUG_TYPE "FunctionalStream"

FunctionalStreamEngine::FunctionalStreamEngine(
    std::unique_ptr<llvm::Interpreter> &_Interpreter)
    : Interpreter(_Interpreter) {}

void FunctionalStreamEngine::configure(Stream *S, DataGraph *DG) {
  auto FS = this->getOrInitializeFunctionalStream(S);
  FS->configure(DG);
}

void FunctionalStreamEngine::step(Stream *S, DataGraph *DG) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {

    // Update the coalesce information for this stream root.
    auto DynamicStreamCoalescer =
        this->getOrInitializeDynamicStreamCoalescer(FS);
    DynamicStreamCoalescer->updateCoalesceMatrix();

    FS->step(DG);
  }
}

void FunctionalStreamEngine::updateLoadedValue(Stream *S, DataGraph *DG,
                                               const DynamicValue &DynamicVal) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {
    FS->updateLoadedValue(DG, DynamicVal);
  }
}

void FunctionalStreamEngine::updatePHINodeValue(
    Stream *S, DataGraph *DG, const DynamicValue &DynamicVal) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {
    FS->updatePHINodeValue(DG, DynamicVal);
  }
}

void FunctionalStreamEngine::endStream(Stream *S) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {
    FS->endStream();
  }
}

void FunctionalStreamEngine::access(Stream *S) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {
    FS->access();
  }
}

FunctionalStream *
FunctionalStreamEngine::getOrInitializeFunctionalStream(Stream *S) {
  auto Iter = this->StreamMap.find(S);
  if (Iter == this->StreamMap.end()) {
    Iter = this->StreamMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(S),
                        std::forward_as_tuple(S, this))
               .first;
  }
  return &(Iter->second);
}

DynamicStreamCoalescer *
FunctionalStreamEngine::getOrInitializeDynamicStreamCoalescer(
    FunctionalStream *FS) {
  auto Iter = this->DynamicStreamCoalescerMap.find(FS);
  if (Iter == this->DynamicStreamCoalescerMap.end()) {
    Iter = this->DynamicStreamCoalescerMap
               .emplace(std::piecewise_construct, std::forward_as_tuple(FS),
                        std::forward_as_tuple(FS))
               .first;
  }
  return &(Iter->second);
}

FunctionalStream *
FunctionalStreamEngine::getNullableFunctionalStream(Stream *S) {
  auto Iter = this->StreamMap.find(S);
  if (Iter == this->StreamMap.end()) {
    return nullptr;
  }
  return &(Iter->second);
}

void FunctionalStreamEngine::finalizeCoalesceInfo() {
  for (auto &DynamicStreamCoalescerPair : this->DynamicStreamCoalescerMap) {
    auto &Coalescer = DynamicStreamCoalescerPair.second;
    Coalescer.finalize();

    for (auto DependentStepFS :
         DynamicStreamCoalescerPair.first->getAllDependentStepStreamsSorted()) {
      auto CoalesceGroup = Coalescer.getCoalesceGroup(DependentStepFS);
      // Update the coalesce group.
      DependentStepFS->getStream()->setCoalesceGroup(CoalesceGroup);
    }
  }
}

#undef DEBUG_TYPE