#include "StreamRegionAnalyzer.h"

#include "llvm/Support/FileSystem.h"

#include "google/protobuf/util/json_util.h"

#include <iomanip>
#include <sstream>

#define DEBUG_TYPE "StreamRegionAnalyzer"

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

  auto ErrCode = llvm::sys::fs::create_directory(this->AnalyzePath);
  assert(!ErrCode && "Failed to create AnalyzePath.");

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
      NewStream =
          new InductionVarStream(this->AnalyzePath, PHIInst, ConfiguredLoop,
                                 InnerMostLoop, LoopLevel, this->DataLayout);
    } else {
      auto IsIVStream = [this](const llvm::PHINode *PHINode) -> bool {
        return this->InstStreamMap.count(PHINode) != 0;
      };
      NewStream =
          new MemStream(this->AnalyzePath, StreamInst, ConfiguredLoop,
                        InnerMostLoop, LoopLevel, this->DataLayout, IsIVStream);
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

  // Finalize all patterns.
  // This will dump the pattern to file.
  for (auto &InstStream : this->InstStreamMap) {
    for (auto &S : InstStream.second) {
      S->finalizePattern();
    }
  }

  this->markQualifiedStreams();
  this->disqualifyStreams();
  if (StreamPassChooseStrategy == StreamPassChooseStrategyE::OUTER_MOST) {
    this->chooseStreamAtOuterMost();
  } else {
    this->chooseStreamAtInnerMost();
  }
  this->buildChosenStreamDependenceGraph();
  this->buildAddressModule();
  this->buildTransformPlan();

  this->dumpStreamInfos();
  this->dumpTransformPlan();
  this->dumpConfigurePlan();
}

void StreamRegionAnalyzer::endTransform() {
  this->FuncSE->endAll();
  for (auto &InstChosenStream : this->InstChosenStreamMap) {
    auto &ChosenStream = InstChosenStream.second;
    ChosenStream->finalizeInfo(this->DataLayout);
  }
}

Stream *StreamRegionAnalyzer::getStreamByInstAndConfiguredLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfiguredLoop) const {
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

  auto AddressModulePath = this->AnalyzePath + "/addr.ll";

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

  // Write it to file for debug purpose.
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(AddressModulePath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  assert(!ModuleFStream.has_error() &&
         "Failed to open the address computation module file.");
  AddressModule->print(ModuleFStream, nullptr);
  ModuleFStream.close();

  // Create the interpreter and functional stream engine.
  this->AddrInterpreter =
      std::make_unique<llvm::Interpreter>(std::move(AddressModule));
  std::unordered_set<Stream *> ChosenStreams;
  for (auto &InstChosenStream : this->InstChosenStreamMap) {
    auto ChosenStream = InstChosenStream.second;
    ChosenStreams.insert(ChosenStream);
  }
  this->FuncSE = std::make_unique<FunctionalStreamEngine>(this->AddrInterpreter,
                                                          ChosenStreams);
}

void StreamRegionAnalyzer::buildTransformPlan() {

  // First initialize the all the plans to nothing.
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      this->InstPlanMap.emplace(std::piecewise_construct,
                                std::forward_as_tuple(Inst),
                                std::forward_as_tuple());
    }
  }

  /**
   * Slice the program and assign transformation plan for static instructions.
   * 1. For every chosen stream S:
   *  a. Mark the stream as
   *    load -> delete, store -> stream-store, phi -> delete.
   *  b. Find the user of the stream and add user information.
   *  c. Find all the compute instructions and mark them as delete candidates.
   *  d. Mark address/phi instruction as deleted.
   *  e. Mark S as processed.
   * 2. Use the deleted dependence edge as the seed, and mark the delete
   * candidates as deleted iff. all the use edges of the candidate are already
   * marked as deleted.
   */
  std::unordered_set<const llvm::Instruction *> DeleteCandidates;
  std::unordered_set<const llvm::Instruction *> DeletedInsts;
  std::list<const llvm::Use *> NewlyDeletingQueue;
  std::unordered_set<const llvm::Use *> DeletedUses;

  for (auto &InstChosenStream : this->InstChosenStreamMap) {

    auto &SelfInst = InstChosenStream.first;
    auto &S = InstChosenStream.second;

    DEBUG(llvm::errs() << "make transform plan for stream " << S->formatName()
                       << '\n');

    // Handle all the step instructions.
    if (S->Type == Stream::TypeT::IV) {
      for (const auto &StepInst : S->getStepInsts()) {
        this->InstPlanMap.at(StepInst).planToStep(S);
        DEBUG(llvm::errs() << "Select transform plan for inst "
                           << LoopUtils::formatLLVMInst(StepInst) << " to "
                           << StreamTransformPlan::formatPlanT(
                                  StreamTransformPlan::PlanT::STEP)
                           << '\n');
        /**
         * A hack here to make the user of step instruction also user of the
         * stream. PURE EVIL!!
         * TODO: Is there better way to do this?
         */
        for (auto U : StepInst->users()) {
          if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
            if (!this->TopLoop->contains(I)) {
              continue;
            }
            this->InstPlanMap.at(I).addUsedStream(S);
          }
        }
      }
    }

    // Add all the compute instructions as delete candidates.
    for (const auto &ComputeInst : S->getComputeInsts()) {
      DeleteCandidates.insert(ComputeInst);
    }

    // Add uses for all the users to use myself.
    for (auto U : SelfInst->users()) {
      if (auto I = llvm::dyn_cast<llvm::Instruction>(U)) {
        if (!this->TopLoop->contains(I)) {
          continue;
        }
        this->InstPlanMap.at(I).addUsedStream(S);
        DEBUG(llvm::errs() << "Add used stream for user "
                           << LoopUtils::formatLLVMInst(I) << " with stream "
                           << S->formatName() << '\n');
      }
    }

    // Mark myself as DELETE (load) or STORE (store).
    auto &SelfPlan = this->InstPlanMap.at(SelfInst);
    if (llvm::isa<llvm::StoreInst>(SelfInst)) {
      assert(SelfPlan.Plan == StreamTransformPlan::PlanT::NOTHING &&
             "Already have a plan for the store.");
      SelfPlan.planToStore(S);
      // For store, only the address use is deleted.
      NewlyDeletingQueue.push_back(&SelfInst->getOperandUse(1));
    } else {
      // For load or iv, delete if we have no other plan for it.
      if (SelfPlan.Plan == StreamTransformPlan::PlanT::NOTHING) {
        SelfPlan.planToDelete();
      }
      for (unsigned OperandIdx = 0, NumOperands = SelfInst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        NewlyDeletingQueue.push_back(&SelfInst->getOperandUse(OperandIdx));
      }
    }

    DEBUG(llvm::errs() << "Select transform plan for inst "
                       << LoopUtils::formatLLVMInst(SelfInst) << " to "
                       << StreamTransformPlan::formatPlanT(SelfPlan.Plan)
                       << '\n');
  }

  // Second step: find deletable instruction from the candidates.
  while (!NewlyDeletingQueue.empty()) {
    auto NewlyDeletingUse = NewlyDeletingQueue.front();
    NewlyDeletingQueue.pop_front();

    if (DeletedUses.count(NewlyDeletingUse) != 0) {
      // We have already process this use.
      continue;
    }

    DeletedUses.insert(NewlyDeletingUse);

    auto NewlyDeletingValue = NewlyDeletingUse->get();
    if (auto NewlyDeletingOperandInst =
            llvm::dyn_cast<llvm::Instruction>(NewlyDeletingValue)) {
      // The operand of the deleting use is an instruction.
      if (!this->TopLoop->contains(NewlyDeletingOperandInst)) {
        // This operand is not within our loop, ignore it.
        continue;
      }
      if (DeleteCandidates.count(NewlyDeletingOperandInst) == 0) {
        // The operand is not even a delete candidate.
        continue;
      }
      if (DeletedInsts.count(NewlyDeletingOperandInst) != 0) {
        // This inst is already deleted.
        continue;
      }
      // Check if all the uses have been deleted.
      bool AllUsesDeleted = true;
      for (const auto &U : NewlyDeletingOperandInst->uses()) {
        if (DeletedUses.count(&U) == 0) {
          AllUsesDeleted = false;
          break;
        }
      }
      if (AllUsesDeleted) {
        // We can safely delete this one now.
        // Actually mark this one as delete if we have no other plan for it.
        auto &Plan = this->InstPlanMap.at(NewlyDeletingOperandInst);
        switch (Plan.Plan) {
        case StreamTransformPlan::PlanT::NOTHING:
        case StreamTransformPlan::PlanT::DELETE: {
          Plan.planToDelete();
          break;
        }
        }
        // Add all the uses to NewlyDeletingQueue.
        for (unsigned OperandIdx = 0,
                      NumOperands = NewlyDeletingOperandInst->getNumOperands();
             OperandIdx != NumOperands; ++OperandIdx) {
          NewlyDeletingQueue.push_back(
              &NewlyDeletingOperandInst->getOperandUse(OperandIdx));
        }
      }
    }
  }
}

void StreamRegionAnalyzer::dumpTransformPlan() {
  std::stringstream ss;
  ss << "DEBUG PLAN FOR LOOP " << LoopUtils::getLoopId(this->TopLoop)
     << "----------------\n";
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    ss << BB->getName().str() << "---------------------------\n";
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      std::string PlanStr = this->InstPlanMap.at(Inst).format();
      ss << std::setw(50) << std::left << LoopUtils::formatLLVMInst(Inst)
         << PlanStr << '\n';
    }
  }

  // Also dump to file.
  std::string PlanPath = this->AnalyzePath + "/plan.txt";
  std::ofstream PlanFStream(PlanPath);
  assert(PlanFStream.is_open() &&
         "Failed to open dump loop transform plan file.");
  PlanFStream << ss.str() << '\n';
  PlanFStream.close();
}

void StreamRegionAnalyzer::dumpConfigurePlan() {
  std::list<const llvm::Loop *> Stack;
  Stack.emplace_back(this->TopLoop);
  std::stringstream ss;
  while (!Stack.empty()) {
    auto Loop = Stack.back();
    Stack.pop_back();
    for (auto &Sub : Loop->getSubLoops()) {
      Stack.emplace_back(Sub);
    }
    ss << LoopUtils::getLoopId(Loop) << '\n';
    for (auto S : this->getSortedChosenStreamsByConfiguredLoop(Loop)) {
      for (auto Level = this->TopLoop->getLoopDepth();
           Level < Loop->getLoopDepth(); ++Level) {
        ss << "  ";
      }
      ss << S->formatName() << '\n';
    }
  }
  std::string PlanPath = this->AnalyzePath + "/config.plan.txt";
  std::ofstream PlanFStream(PlanPath);
  assert(PlanFStream.is_open() &&
         "Failed to open dump loop configure plan file.");
  PlanFStream << ss.str() << '\n';
  PlanFStream.close();
}

std::list<Stream *>
StreamRegionAnalyzer::getSortedChosenStreamsByConfiguredLoop(
    const llvm::Loop *ConfiguredLoop) {
  assert(this->TopLoop->contains(ConfiguredLoop) &&
         "ConfiguredLoop should be within TopLoop.");

  // Topological sort.
  std::stack<std::pair<Stream *, int>> ChosenStreams;
  for (auto &InstChosenStream : this->InstChosenStreamMap) {
    auto &S = InstChosenStream.second;
    if (S->getLoop() == ConfiguredLoop) {
      ChosenStreams.emplace(S, 0);
    }
  }

  std::unordered_set<Stream *> Inserted;
  std::list<Stream *> SortedStreams;
  while (!ChosenStreams.empty()) {
    auto &Entry = ChosenStreams.top();
    auto &S = Entry.first;
    if (Entry.second == 0) {
      Entry.second = 1;
      for (auto &ChosenBaseS : S->getChosenBaseStreams()) {
        if (ChosenBaseS->getLoop() != ConfiguredLoop) {
          continue;
        }
        if (Inserted.count(ChosenBaseS) == 0) {
          ChosenStreams.emplace(ChosenBaseS, 0);
        }
      }
    } else {
      if (Inserted.count(S) == 0) {
        SortedStreams.emplace_back(S);
        Inserted.insert(S);
      }
      ChosenStreams.pop();
    }
  }

  return SortedStreams;
}

Stream *
StreamRegionAnalyzer::getChosenStreamByInst(const llvm::Instruction *Inst) {
  if (!this->TopLoop->contains(Inst)) {
    return nullptr;
  }
  auto Iter = this->InstChosenStreamMap.find(Inst);
  if (Iter == this->InstChosenStreamMap.end()) {
    return nullptr;
  } else {
    return Iter->second;
  }
}

const StreamTransformPlan &
StreamRegionAnalyzer::getTransformPlanByInst(const llvm::Instruction *Inst) {
  assert(this->TopLoop->contains(Inst) && "Inst should be within the TopLoop.");
  return this->InstPlanMap.at(Inst);
}

FunctionalStreamEngine *StreamRegionAnalyzer::getFuncSE() {
  return this->FuncSE.get();
}

std::string StreamRegionAnalyzer::classifyStream(const MemStream &S) const {

  if (S.getNumBaseLoads() > 0) {
    // We have dependent base loads here.
    return "INDIRECT";
  }

  // No base loads.
  const auto &BaseInductionVars = S.getBaseInductionVars();
  if (BaseInductionVars.empty()) {
    // No base ivs, should be a constant stream, but here we treat it as AFFINE.
    return "AFFINE";
  }

  if (BaseInductionVars.size() > 1) {
    // Multiple IVs.
    return "MULTI_IV";
  }

  const auto &BaseIV = *BaseInductionVars.begin();
  auto BaseIVStream =
      this->getStreamByInstAndConfiguredLoop(BaseIV, S.getLoop());
  if (BaseIVStream == nullptr) {
    // This should not happen, but so far make it indirect.
    return "INDIRECT";
  }

  const auto &BaseIVBaseLoads = BaseIVStream->getBaseLoads();
  if (!BaseIVBaseLoads.empty()) {
    for (const auto &BaseIVBaseLoad : BaseIVBaseLoads) {
      if (BaseIVBaseLoad == S.getInst()) {
        return "POINTER_CHASE";
      }
    }
    return "INDIRECT";
  }

  if (BaseIVStream->getPattern().computed()) {
    auto Pattern = BaseIVStream->getPattern().getPattern().ValPattern;
    if (Pattern <= StreamPattern::ValuePattern::QUARDRIC) {
      // The pattern of the IV is AFFINE.
      return "AFFINE";
    }
  }

  return "RANDOM";
}

void StreamRegionAnalyzer::dumpStats() const {

  std::string StatFilePath = this->AnalyzePath + "/stats.txt";

  std::ofstream O(StatFilePath);
  assert(O.is_open() && "Failed to open stats file.");

  O << "--------------- Stats -----------------\n";
  /**
   * Loop
   * Inst
   * AddrPat
   * AccPat
   * Iters
   * Accs
   * Updates
   * StreamCount
   * LoopLevel
   * BaseLoads
   * StreamClass
   * Footprint
   * AddrInsts
   * AliasInsts
   * Qualified
   * Chosen
   * PossiblePaths
   */

  O << "--------------- Stream -----------------\n";
  for (auto &InstStreamEntry : this->InstStreamMap) {
    if (llvm::isa<llvm::PHINode>(InstStreamEntry.first)) {
      continue;
    }
    for (auto &S : InstStreamEntry.second) {
      auto &Stream = *reinterpret_cast<MemStream *>(S);
      O << LoopUtils::getLoopId(Stream.getLoop());
      O << ' ' << LoopUtils::formatLLVMInst(Stream.getInst());

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = Stream.getTotalIters();
      size_t Accesses = Stream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = Stream.getTotalStreams();
      size_t LoopLevel = Stream.getLoopLevel();
      size_t BaseLoads = Stream.getNumBaseLoads();

      std::string StreamClass = this->classifyStream(Stream);
      size_t Footprint = Stream.getFootprint().getNumCacheLinesAccessed();
      size_t AddrInsts = Stream.getNumAddrInsts();

      std::string Qualified = "NO";
      if (Stream.getQualified()) {
        Qualified = "YES";
      }

      std::string Chosen = Stream.isChosen() ? "YES" : "NO";

      int LoopPossiblePaths = -1;

      size_t AliasInsts = Stream.getAliasInsts().size();

      if (Stream.getPattern().computed()) {
        const auto &Pattern = Stream.getPattern().getPattern();
        AddrPattern = StreamPattern::formatValuePattern(Pattern.ValPattern);
        AccPattern = StreamPattern::formatAccessPattern(Pattern.AccPattern);
        Updates = Pattern.Updates;
      }
      O << ' ' << AddrPattern;
      O << ' ' << AccPattern;
      O << ' ' << Iters;
      O << ' ' << Accesses;
      O << ' ' << Updates;
      O << ' ' << StreamCount;
      O << ' ' << LoopLevel;
      O << ' ' << BaseLoads;
      O << ' ' << StreamClass;
      O << ' ' << Footprint;
      O << ' ' << AddrInsts;
      O << ' ' << AliasInsts;
      O << ' ' << Qualified;
      O << ' ' << Chosen;
      O << ' ' << LoopPossiblePaths;
      O << '\n';
    }
  }

  O << "--------------- Loop -----------------\n";
  O << "--------------- IV Stream ------------\n";
  for (auto &InstStreamEntry : this->InstStreamMap) {
    if (!llvm::isa<llvm::PHINode>(InstStreamEntry.first)) {
      continue;
    }
    for (auto &S : InstStreamEntry.second) {
      auto &IVStream = *reinterpret_cast<InductionVarStream *>(S);
      O << LoopUtils::getLoopId(IVStream.getLoop());
      O << ' ' << LoopUtils::formatLLVMInst(IVStream.getPHIInst());

      std::string AddrPattern = "NOT_COMPUTED";
      std::string AccPattern = "NOT_COMPUTED";
      size_t Iters = IVStream.getTotalIters();
      size_t Accesses = IVStream.getTotalAccesses();
      size_t Updates = 0;
      size_t StreamCount = IVStream.getTotalStreams();

      size_t ComputeInsts = IVStream.getComputeInsts().size();

      std::string StreamClass = "whatever";
      if (IVStream.getPattern().computed()) {
        const auto &Pattern = IVStream.getPattern().getPattern();
        AddrPattern = StreamPattern::formatValuePattern(Pattern.ValPattern);
        AccPattern = StreamPattern::formatAccessPattern(Pattern.AccPattern);
        Updates = Pattern.Updates;
      }

      int LoopPossiblePaths = -1;

      O << ' ' << AddrPattern;
      O << ' ' << AccPattern;
      O << ' ' << Iters;
      O << ' ' << Accesses;
      O << ' ' << Updates;
      O << ' ' << StreamCount;
      O << ' ' << StreamClass;
      O << ' ' << ComputeInsts;
      O << ' ' << LoopPossiblePaths;
      O << '\n';
    }
  }
  O.close();
}

#undef DEBUG_TYPE