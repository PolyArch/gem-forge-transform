#include "StreamRegionAnalyzer.h"

#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

StreamRegionAnalyzer::StreamRegionAnalyzer(uint64_t _RegionIdx,
                                           llvm::Loop *_TopLoop,
                                           llvm::LoopInfo *_LI,
                                           const std::string &_RootPath)
    : RegionIdx(_RegionIdx), TopLoop(_TopLoop), LI(_LI), RootPath(_RootPath) {
  // Initialize the folder for this region.
  std::stringstream ss;
  ss << "R." << this->RegionIdx << ".A." << LoopUtils::getLoopId(this->TopLoop);
  this->AnalyzePath = this->RootPath + "/" + ss.str();

  struct stat st = {0};
  if (stat(this->AnalyzePath.c_str(), &st) == -1) {
    mkdir(this->AnalyzePath.c_str(), 0700);
  } else {
    assert(false && "The analyze folder already exists.");
  }

  this->initializeStreams();
}

StreamRegionAnalyzer::~StreamRegionAnalyzer() {
  for (auto &InstStreams : this->InstStreamMap) {
    for (auto &Stream : InstStreams.second) {
      delete Stream;
      Stream = nullptr;
    }
    InstStreams.second.clear();
  }
  this->InstStreamMap.clear();
}

void StreamRegionAnalyzer::initializeStreams() {

  /**
   * TODO: Consider memorizing this.
   */

  {
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
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        auto StaticInst = &*InstIter;
        if (!Utils::isMemAccessInst(StaticInst)) {
          continue;
        }
        this->initializeStreamForAllLoops(StaticInst);
      }
    }
  }
}

void StreamRegionAnalyzer::initializeStreamForAllLoops(
    const llvm::Instruction *StreamInst) {
  auto InnerMostLoop = this->LI->getLoopFor(StreamInst->getParent());
  assert(InnerMostLoop != nullptr &&
         "Failed to get inner most loop for stream inst.");
  assert(this->TopLoop->contains(InnerMostLoop) &&
         "Stream inst is not within top loop.");

  auto &Streams =
      this->InstStreamMap
          .emplace(std::piecewise_construct, std::forward_as_tuple(StreamInst),
                   std::forward_as_tuple())
          .first->second;
  assert(Streams.empty() &&
         "There is already streams initialized for the stream instruction.");

  auto ConfiguredLoop = InnerMostLoop;
  do {
    auto LoopLevel =
        ConfiguredLoop->getLoopDepth() - this->TopLoop->getLoopDepth();
    Stream *NewStream = nullptr;
    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
      NewStream = new InductionVarStream(
          this->AnalyzePath, PHIInst, ConfiguredLoop, InnerMostLoop, LoopLevel);
    } else {
      auto IsIVStream = [this](const llvm::PHINode *PHINode) -> bool {
        return this->InstStreamMap.count(PHINode) != 0;
      };
      NewStream = new MemStream(this->AnalyzePath, StreamInst, ConfiguredLoop,
                                InnerMostLoop, LoopLevel, IsIVStream);
    }
    Streams.emplace_back(NewStream);

    // Update the ConfguredLoopStreamMap.
    this->ConfiguredLoopStreamMap
        .emplace(std::piecewise_construct,
                 std::forward_as_tuple(ConfiguredLoop), std::forward_as_tuple())
        .first->second.insert(NewStream);
    this->InnerMostLoopStreamMap
        .emplace(std::piecewise_construct, std::forward_as_tuple(InnerMostLoop),
                 std::forward_as_tuple())
        .first->second.insert(NewStream);

    ConfiguredLoop = ConfiguredLoop->getParentLoop();
  } while (this->TopLoop->contains(ConfiguredLoop));
}

void StreamRegionAnalyzer::addMemAccess(DynamicInstruction *DynamicInst,
                                        DataGraph *DG) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  assert(Utils::isMemAccessInst(StaticInst) &&
         "Should be memory access instruction.");
  auto Iter = this->InstStreamMap.find(StaticInst);
  assert(Iter != this->InstStreamMap.end() && "Failed to find the stream.");
  uint64_t Addr = Utils::getMemAddr(DynamicInst);
  for (auto &Stream : Iter->second) {
    Stream->addAccess(Addr, DynamicInst->getId());
  }

  // Handle the aliasing.
  auto MemDepsIter = DG->MemDeps.find(DynamicInst->getId());
  if (MemDepsIter == DG->MemDeps.end()) {
    // There is no known alias.
    return;
  }
  for (const auto &MemDepId : MemDepsIter->second) {
    // Check if this dynamic instruction is still alive.
    auto MemDepDynamicInst = DG->getAliveDynamicInst(MemDepId);
    if (MemDepDynamicInst == nullptr) {
      // This dynamic instruction is already committed. We assume that the
      // dependence is old enough to be safely ignored.
      continue;
    }
    auto MemDepStaticInst = MemDepDynamicInst->getStaticInstruction();
    assert(MemDepStaticInst != nullptr &&
           "Invalid memory dependent llvm static instruction.");

    for (auto &S : Iter->second) {
      auto MStream = reinterpret_cast<MemStream *>(S);
      const auto ConfiguredLoop = MStream->getLoop();
      const auto StartId = MStream->getStartId();
      if (StartId == DynamicInstruction::InvalidId) {
        // This stream is not even active (or more precisely, has not seen the
        // first access).
        continue;
      }
      // Ignore those instruction not from our configuration loop.
      if (!ConfiguredLoop->contains(MemDepStaticInst)) {
        continue;
      }
      // Check if they are older than the start id.
      if (MemDepId < StartId) {
        continue;
      }
      MStream->addAliasInst(MemDepStaticInst);
    }
  }
}

void StreamRegionAnalyzer::addIVAccess(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr && "Invalid llvm static instruction.");
  for (unsigned OperandIdx = 0, NumOperands = StaticInst->getNumOperands();
       OperandIdx != NumOperands; ++OperandIdx) {
    auto OperandValue = StaticInst->getOperand(OperandIdx);
    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(OperandValue)) {
      auto Iter = this->InstStreamMap.find(PHINode);
      if (Iter != this->InstStreamMap.end()) {
        const auto &DynamicOperand =
            *(DynamicInst->DynamicOperands[OperandIdx]);
        for (auto &Stream : Iter->second) {
          Stream->addAccess(DynamicOperand);
        }
      } else {
        // This means that this is not an IV stream.
      }
    }
  }
}

void StreamRegionAnalyzer::endIter(const llvm::Loop *Loop) {
  assert(this->TopLoop->contains(Loop) &&
         "End iteration for loop outside of TopLoop.");
  auto Iter = this->InnerMostLoopStreamMap.find(Loop);
  if (Iter != this->InnerMostLoopStreamMap.end()) {
    for (auto &Stream : Iter->second) {
      Stream->endIter();
    }
  }
}

void StreamRegionAnalyzer::endLoop(const llvm::Loop *Loop) {
  assert(this->TopLoop->contains(Loop) &&
         "End loop for loop outside of TopLoop.");
  auto Iter = this->ConfiguredLoopStreamMap.find(Loop);
  if (Iter != this->ConfiguredLoopStreamMap.end()) {
    for (auto &Stream : Iter->second) {
      Stream->endStream();
    }
  }
}

void StreamRegionAnalyzer::endRegion() {}