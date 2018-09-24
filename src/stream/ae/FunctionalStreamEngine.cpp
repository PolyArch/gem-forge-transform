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

void FunctionalStreamEngine::endStream(Stream *S) {
  auto FS = this->getNullableFunctionalStream(S);
  if (FS != nullptr) {
    FS->endStream();
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

FunctionalStream *
FunctionalStreamEngine::getNullableFunctionalStream(Stream *S) {
  auto Iter = this->StreamMap.find(S);
  if (Iter == this->StreamMap.end()) {
    return nullptr;
  }
  return &(Iter->second);
}

#undef DEBUG_TYPE