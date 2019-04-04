#include "StaticStreamRegionAnalyzer.h"

#include "Utils.h"

StaticStreamRegionAnalyzer::StaticStreamRegionAnalyzer(
    llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
    CachedLoopInfo *_CachedLI)
    : TopLoop(_TopLoop), DataLayout(_DataLayout), CachedLI(_CachedLI),
      LI(_CachedLI->getLoopInfo(_TopLoop->getHeader()->getParent())),
      SE(_CachedLI->getScalarEvolution(_TopLoop->getHeader()->getParent())) {
  this->initializeStreams();
  this->buildStreamDependenceGraph();
}

StaticStreamRegionAnalyzer::~StaticStreamRegionAnalyzer() {
  for (auto &InstStreams : this->InstStaticStreamMap) {
    for (auto &Stream : InstStreams.second) {
      delete Stream;
      Stream = nullptr;
    }
    InstStreams.second.clear();
  }
  this->InstStaticStreamMap.clear();
}

void StaticStreamRegionAnalyzer::initializeStreams() {

  /**
   * Do a BFS to find all the IV streams in the header of both this loop and
   * subloops.
   */
  std::list<llvm::Loop *> Queue;
  Queue.emplace_back(this->TopLoop);
  while (!Queue.empty()) {
    auto CurrentLoop = Queue.front();
    Queue.pop_front();
    for (auto &SubLoop : CurrentLoop->getSubLoops()) {
      Queue.emplace_back(SubLoop);
    }
    auto HeaderBB = CurrentLoop->getHeader();
    auto PHINodes = HeaderBB->phis();
    for (auto PHINodeIter = PHINodes.begin(), PHINodeEnd = PHINodes.end();
         PHINodeIter != PHINodeEnd; ++PHINodeIter) {
      auto PHINode = &*PHINodeIter;
      this->initializeStreamForAllLoops(PHINode);
    }
  }

  /**
   * Find all the memory stream instructions.
   */
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto StaticInst = &*InstIter;
      if (!Utils::isMemAccessInst(StaticInst)) {
        continue;
      }
      this->initializeStreamForAllLoops(StaticInst);
    }
  }
}

void StaticStreamRegionAnalyzer::initializeStreamForAllLoops(
    llvm::Instruction *StreamInst) {
  auto InnerMostLoop = this->LI->getLoopFor(StreamInst->getParent());
  assert(InnerMostLoop != nullptr &&
         "Failed to get inner most loop for stream inst.");
  assert(this->TopLoop->contains(InnerMostLoop) &&
         "Stream inst is not within top loop.");

  auto &Streams =
      this->InstStaticStreamMap
          .emplace(std::piecewise_construct, std::forward_as_tuple(StreamInst),
                   std::forward_as_tuple())
          .first->second;
  assert(Streams.empty() &&
         "There is already streams initialized for the stream instruction.");

  auto ConfigureLoop = InnerMostLoop;
  do {
    auto LoopLevel =
        ConfigureLoop->getLoopDepth() - this->TopLoop->getLoopDepth();
    StaticStream *NewStream = nullptr;
    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
      NewStream =
          new StaticIndVarStream(PHIInst, ConfigureLoop, InnerMostLoop, SE);
    } else {
      NewStream =
          new StaticMemStream(StreamInst, ConfigureLoop, InnerMostLoop, SE);
    }
    Streams.emplace_back(NewStream);

    ConfigureLoop = ConfigureLoop->getParentLoop();
  } while (this->TopLoop->contains(ConfigureLoop));
}

StaticStream *StaticStreamRegionAnalyzer::getStreamByInstAndConfigureLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfigureLoop) const {
  auto Iter = this->InstStaticStreamMap.find(Inst);
  if (Iter == this->InstStaticStreamMap.end()) {
    return nullptr;
  }
  const auto &Streams = Iter->second;
  for (const auto &S : Streams) {
    if (S->ConfigureLoop == ConfigureLoop) {
      return S;
    }
  }
  llvm_unreachable("Failed to find the stream at specified loop level.");
}

void StaticStreamRegionAnalyzer::buildStreamDependenceGraph() {
  auto GetStream = [this](const llvm::Instruction *Inst,
                          const llvm::Loop *ConfigureLoop) -> StaticStream * {
    return this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop);
  };
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->buildDependenceGraph(GetStream);
    }
  }
  /**
   * After add all the base streams, we are going to compute the base step root
   * streams. The computeBaseStepRootStreams() is by itself recursive. This will
   * result in some overhead, but hopefully the dependency chain is not very
   * long.
   */
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->computeBaseStepRootStreams();
    }
  }
}