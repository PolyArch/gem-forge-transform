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

  /**
   * Group FunctionalStream into potential coalescing group based on:
   * 1. Direct memory streams.
   * 2. Same configure loop.
   * 3. Same step root stream.
   *
   * Usually the number pair of <configure_loop, step_root_stream> is small, so
   * we do not bother defining a hash function for std::unordered_map. Instead
   * we use std::map.
   */
  std::map<std::pair<const llvm::Loop *, const Stream *>,
           std::unordered_set<FunctionalStream *>>
      PotentialCoalescingGroups;
  auto IsDirectMemWithStepStream = [](const Stream *S) -> bool {
    if (S->SStream->Type != StaticStream::TypeT::MEM) {
      return false;
    }
    for (auto BaseS : S->getChosenBaseStepStreams()) {
      if (BaseS->SStream->Type != StaticStream::TypeT::IV) {
        // Base on a mem stream.
        if (BaseS->getChosenBaseStepStreams().empty()) {
          // This is a constant load. Ignore it.
          continue;
        }
        // Indirect stream.
        return false;
      }
    }
    if (S->getSingleChosenStepRootStream() == nullptr) {
      // This stream will never step.
      return false;
    }
    return true;
  };
  for (auto &StreamFunctionalStream : this->StreamMap) {
    auto &FS = StreamFunctionalStream.second;
    auto &S = StreamFunctionalStream.first;
    if (!IsDirectMemWithStepStream(S)) {
      continue;
    }
    // Add to our potential coalescing group map.
    PotentialCoalescingGroups
        .emplace(std::piecewise_construct,
                 std::forward_as_tuple(S->getLoop(),
                                       S->getSingleChosenStepRootStream()),
                 std::forward_as_tuple())
        .first->second.insert(&FS);
  }
  // Create the coalescer.
  for (const auto &PotentialCoalesceGroup : PotentialCoalescingGroups) {
    auto RootS = PotentialCoalesceGroup.first.second;
    auto RootFS = &this->StreamMap.at(RootS);
    this->DynamicStreamCoalescerMap
        .emplace(std::piecewise_construct, std::forward_as_tuple(RootFS),
                 std::forward_as_tuple())
        .first->second.emplace_back(PotentialCoalesceGroup.second);
    // llvm::errs() << "Create coalescer for " << RootS->formatName() << '\n';
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

  // Update the coalesce information for this stream root.
  if (this->DynamicStreamCoalescerMap.count(&FS) != 0) {
    for (auto &Coalescer : this->DynamicStreamCoalescerMap.at(&FS)) {
      Coalescer.updateCoalesceMatrix();
    }
    // llvm::errs() << "Update coalescer for stream "
    //              << FS.getStream()->formatName() << '\n';
  }
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

void FunctionalStreamEngine::finalizeCoalesceInfo() {
  for (auto &StepRootStreamCoalescers : this->DynamicStreamCoalescerMap) {
    for (auto &Coalescer : StepRootStreamCoalescers.second) {
      Coalescer.finalize();
    }
    // llvm::errs() << "Finalize coalescer for stream "
    //              << StepRootStreamCoalescers.first->getStream()->formatName()
    //              << '\n';
  }
}

#undef DEBUG_TYPE
