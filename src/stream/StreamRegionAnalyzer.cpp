#include "StreamRegionAnalyzer.h"

#include "llvm/Support/FileSystem.h"

#include "google/protobuf/util/json_util.h"

#include <sstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

StreamRegionAnalyzer::StreamRegionAnalyzer(uint64_t _RegionIdx,
                                           llvm::Loop *_TopLoop,
                                           llvm::LoopInfo *_LI,
                                           llvm::DataLayout *_DataLayout,
                                           const std::string &_RootPath)
    : RegionIdx(_RegionIdx), TopLoop(_TopLoop), LI(_LI),
      DataLayout(_DataLayout), RootPath(_RootPath) {
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
  this->buildStreamDependenceGraph();
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

void StreamRegionAnalyzer::buildStreamDependenceGraph() {
  auto GetStream = [this](const llvm::Instruction *Inst,
                          const llvm::Loop *ConfiguredLoop) -> Stream * {
    return this->getStreamByInstAndConfiguredLoop(Inst, ConfiguredLoop);
  };
  for (auto &InstStream : this->InstStreamMap) {
    for (auto &S : InstStream.second) {
      S->buildBasicDependenceGraph(GetStream);
    }
  }
  /**
   * After add all the base streams, we are going to compute the base step root
   * streams. The computeBaseStepRootStreams() is by itself recursive. This will
   * result in some overhead, but hopefully the dependency chain is not very
   * long.
   */
  for (auto &InstStream : this->InstStreamMap) {
    for (auto &S : InstStream.second) {
      S->computeBaseStepRootStreams();
    }
  }
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

void StreamRegionAnalyzer::endRegion(
    StreamPassChooseStrategyE StreamPassChooseStrategy) {
  this->markQualifiedStreams();
  this->disqualifyStreams();
  if (StreamPassChooseStrategy == StreamPassChooseStrategyE::OUTER_MOST) {
    this->chooseStreamAtOuterMost();
  } else {
    this->chooseStreamAtInnerMost();
  }
  this->buildChosenStreamDependenceGraph();
  this->buildAddressModule();

  this->dumpStreamInfos();
}

Stream *StreamRegionAnalyzer::getStreamByInstAndConfiguredLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfiguredLoop) {
  auto Iter = this->InstStreamMap.find(Inst);
  if (Iter == this->InstStreamMap.end()) {
    return nullptr;
  }
  const auto &Streams = Iter->second;
  for (const auto &S : Streams) {
    if (S->getLoop() == ConfiguredLoop) {
      return S;
    }
  }
  llvm_unreachable("Failed to find the stream at specified loop level.");
}

void StreamRegionAnalyzer::markQualifiedStreams() {

  std::list<Stream *> Queue;
  for (auto &InstStream : this->InstStreamMap) {
    for (auto &S : InstStream.second) {
      if (S->isQualifySeed()) {
        Queue.emplace_back(S);
      }
    }
  }

  /**
   * BFS on the qualified streams.
   */
  while (!Queue.empty()) {
    auto S = Queue.front();
    Queue.pop_front();
    if (S->isQualified()) {
      // We have already processed this stream.
      continue;
    }
    if (!S->isQualifySeed()) {
      assert(false && "Stream should be a qualify seed to be inserted into the "
                      "qualifying queue.");
      continue;
    }
    S->markQualified();
    // Check all the dependent streams.
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (DependentStream->isQualifySeed()) {
        Queue.emplace_back(DependentStream);
      }
    }
  }
}

void StreamRegionAnalyzer::disqualifyStreams() {

  /**
   * For IVStreams with base loads, we assume they are qualified from the
   * beginning point. Now it's time to check if their base memory streams are
   * actually qualified. If not, we need to propagate the dequalified signal
   * along the dependence chain.
   */
  std::list<Stream *> DisqualifiedQueue;
  for (auto &InstStream : this->InstStreamMap) {
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        continue;
      }
      for (auto &BackIVDependentStream : S->getBackIVDependentStreams()) {
        if (BackIVDependentStream->isQualified()) {
          // We should disqualify this stream.
          DisqualifiedQueue.emplace_back(BackIVDependentStream);
        }
      }
    }
  }

  /**
   * Propagate the disqualfied signal.
   */
  while (!DisqualifiedQueue.empty()) {
    auto S = DisqualifiedQueue.front();
    DisqualifiedQueue.pop_front();
    if (!S->isQualified()) {
      // This stream is already disqualified.
      continue;
    }
    S->markUnqualified();

    // Propagate to dependent streams.
    for (const auto &DependentStream : S->getDependentStreams()) {
      if (DependentStream->isQualified()) {
        DisqualifiedQueue.emplace_back(DependentStream);
      }
    }

    // Also need to check back edges.
    for (const auto &DependentStream : S->getBackIVDependentStreams()) {
      if (DependentStream->isQualified()) {
        DisqualifiedQueue.emplace_back(DependentStream);
      }
    }
  }
}

void StreamRegionAnalyzer::chooseStreamAtInnerMost() {
  for (auto &InstStream : this->InstStreamMap) {
    auto Inst = InstStream.first;
    Stream *ChosenStream = nullptr;
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        ChosenStream = S;
        break;
      }
    }
    if (ChosenStream != nullptr) {
      auto Inserted =
          this->InstChosenStreamMap.emplace(Inst, ChosenStream).second;
      assert(Inserted && "Multiple chosen streams for one instruction.");
      ChosenStream->markChosen();
    }
  }
}

void StreamRegionAnalyzer::chooseStreamAtOuterMost() {
  for (auto &InstStream : this->InstStreamMap) {
    auto Inst = InstStream.first;
    Stream *ChosenStream = nullptr;
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        // This will make sure we get the outer most qualified stream.
        ChosenStream = S;
      }
    }
    if (ChosenStream != nullptr) {
      this->InstChosenStreamMap.emplace(Inst, ChosenStream);
      ChosenStream->markChosen();
    }
  }
}

void StreamRegionAnalyzer::buildChosenStreamDependenceGraph() {
  auto GetChosenStream = [this](const llvm::Instruction *Inst) -> Stream * {
    auto Iter = this->InstChosenStreamMap.find(Inst);
    if (Iter == this->InstChosenStreamMap.end()) {
      return nullptr;
    }
    return Iter->second;
  };
  for (auto &InstStream : this->InstChosenStreamMap) {
    auto &S = InstStream.second;
    S->buildChosenDependenceGraph(GetChosenStream);
  }
}

void StreamRegionAnalyzer::dumpStreamInfos() {
  {
    LLVM::TDG::StreamRegion ProtobufStreamRegion;
    for (auto &InstStream : this->InstStreamMap) {
      for (auto &S : InstStream.second) {
        auto Info = ProtobufStreamRegion.add_streams();
        S->fillProtobufStreamInfo(this->DataLayout, Info);
      }
    }

    auto InfoTextPath = this->AnalyzePath + "/streams.json";
    std::ofstream InfoTextFStream(InfoTextPath);
    assert(InfoTextFStream.is_open() && "Failed to open the output info file.");
    std::string InfoJsonString;
    google::protobuf::util::MessageToJsonString(ProtobufStreamRegion,
                                                &InfoJsonString);
    InfoTextFStream << InfoJsonString << '\n';
    InfoTextFStream.close();
  }

  {
    LLVM::TDG::StreamRegion ProtobufStreamRegion;
    for (auto &InstStream : this->InstChosenStreamMap) {
      auto &S = InstStream.second;
      auto Info = ProtobufStreamRegion.add_streams();
      S->fillProtobufStreamInfo(this->DataLayout, Info);
    }

    auto InfoTextPath = this->AnalyzePath + "/chosen_streams.json";
    std::ofstream InfoTextFStream(InfoTextPath);
    assert(InfoTextFStream.is_open() && "Failed to open the output info file.");
    std::string InfoJsonString;
    google::protobuf::util::MessageToJsonString(ProtobufStreamRegion,
                                                &InfoJsonString);
    InfoTextFStream << InfoJsonString << '\n';
    InfoTextFStream.close();
  }
}

void StreamRegionAnalyzer::buildAddressModule() {

  auto AddressModulePath = this->AnalyzePath + "/addr.bc";

  auto &Context = this->TopLoop->getHeader()->getParent()->getContext();
  auto AddressModule =
      std::make_unique<llvm::Module>(AddressModulePath, Context);

  for (auto &InstStream : this->InstChosenStreamMap) {
    auto Inst = InstStream.first;
    if (llvm::isa<llvm::PHINode>(Inst)) {
      // If this is an IVStream.
      continue;
    }
    auto MStream = reinterpret_cast<MemStream *>(InstStream.second);
    MStream->generateComputeFunction(AddressModule);
  }

  // Write it to file.
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(AddressModulePath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  assert(!ModuleFStream.has_error() &&
         "Failed to open the address computation module file.");
  AddressModule->print(ModuleFStream, nullptr);
  ModuleFStream.close();
}