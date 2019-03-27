#include "FunctionalStreamEngine.h"

#define DEBUG_TYPE "FunctionalStream"

FunctionalStreamEngine::FunctionalStreamEngine(
    std::unique_ptr<llvm::Interpreter> &_Interpreter,
    const std::unordered_set<Stream *> &_ChosenStreams)
    : Interpreter(_Interpreter) {

  // Initialize all functional streams.
  for (auto &S : _ChosenStreams) {
    this->StreamMap.emplace(std::piecewise_construct, std::forward_as_tuple(S),
                            std::forward_as_tuple(S, this));
  }

  // Initialize the dependence graph between functional streams.
  for (auto &StreamFunctionalStream : this->StreamMap) {
    auto &FS = StreamFunctionalStream.second;
    auto &S = StreamFunctionalStream.first;
    for (auto &BaseS : S->getChosenBaseStreams()) {
      auto &BaseFS = this->StreamMap.at(BaseS);
      FS.addBaseStream(&BaseFS);
    }
  }
}

void FunctionalStreamEngine::configure(Stream *S, DataGraph *DG) {
  auto &FS = this->StreamMap.at(S);
  FS.configure(DG);
}

void FunctionalStreamEngine::step(Stream *S, DataGraph *DG) {

  /**
   * Propagate the step signal along the dependence chain, BFS.
   */
  std::unordered_set<FunctionalStream *> SteppedFS;
  std::list<FunctionalStream *> StepQueue;

  auto &FS = this->StreamMap.at(S);
  StepQueue.emplace_back(&FS);

  while (!StepQueue.empty()) {
    auto FS = StepQueue.front();
    StepQueue.pop_front();
    if (SteppedFS.count(FS) != 0) {
      // Already stepped.
      continue;
    }
    FS->step(DG);
    SteppedFS.insert(FS);
    // Add all dependent FS to the queue.
    for (auto &DependentFunctionalStream :
         FS->getDependentFunctionalStreams()) {
      if (DependentFunctionalStream->getStream()->getInnerMostLoop() ==
          S->getInnerMostLoop()) {
        // This dependent stream should be stepped by us.
        if (SteppedFS.count(DependentFunctionalStream) == 0) {
          StepQueue.emplace_back(DependentFunctionalStream);
        }
      }
    }
  }

  // // Update the coalesce information for this stream root.
  // auto DynamicStreamCoalescer =
  //     this->getOrInitializeDynamicStreamCoalescer(&FS);
  // DynamicStreamCoalescer->updateCoalesceMatrix();
}

void FunctionalStreamEngine::updateWithValue(Stream *S, DataGraph *DG,
                                             const DynamicValue &DynamicVal) {
  auto &FS = this->StreamMap.at(S);
  FS.setValue(DynamicVal);

  /**
   * BFS to update all dependent streams.
   * In the case of A-------->B
   *                 \-->C-->/
   * We may update C twice.
   */
  std::list<FunctionalStream *> UpdateQueue;
  UpdateQueue.emplace_back(&FS);

  int Count = 0;
  while (!UpdateQueue.empty()) {
    auto FS = UpdateQueue.front();
    UpdateQueue.pop_front();
    Count++;
    // A definitely upper bound.
    // With n streams, at most n^2 updates.
    if (Count == this->StreamMap.size() * this->StreamMap.size() + 1) {
      llvm::errs() << "Potential cycle dependence found when updating for "
                   << S->formatName() << '\n';
      assert(false && "Potential cycle dependence found.\n");
    }
    FS->update(DG);
    // Add all dependent FS to the queue.
    for (auto &DependentFS : FS->getDependentFunctionalStreams()) {
      UpdateQueue.emplace_back(DependentFS);
    }
  }
}

void FunctionalStreamEngine::endStream(Stream *S) {
  auto &FS = this->StreamMap.at(S);
  FS.endStream();
}

void FunctionalStreamEngine::endAll() {
  for (auto &StreamFunctionalStream : this->StreamMap) {
    auto &FS = StreamFunctionalStream.second;
    FS.endAll();
  }
}

void FunctionalStreamEngine::access(Stream *S) {
  auto &FS = this->StreamMap.at(S);
  FS.access();
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

void FunctionalStreamEngine::finalizeCoalesceInfo() {
  // for (auto &DynamicStreamCoalescerPair : this->DynamicStreamCoalescerMap) {
  //   auto &Coalescer = DynamicStreamCoalescerPair.second;
  //   Coalescer.finalize();

  //   for (auto DependentStepFS :
  //        DynamicStreamCoalescerPair.first->getAllDependentStepStreamsSorted())
  //        {
  //     auto CoalesceGroup = Coalescer.getCoalesceGroup(DependentStepFS);
  //     // Update the coalesce group.
  //     DependentStepFS->getStream()->setCoalesceGroup(CoalesceGroup);
  //   }
  // }
}

#undef DEBUG_TYPE
