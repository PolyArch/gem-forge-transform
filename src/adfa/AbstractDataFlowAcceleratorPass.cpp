/**
 * This file implements an non-speculative abstract datagraph accelerator for
 * inner most loops. Details:
 * 1. Convert the control dependence (postdominance frontier) to data
 * dependence.
 * 2. The original single instruction stream is splited into two streams:
 * the GPP stream and the accelerator (data flow) stream. The GPP stream
 * contains special configure and start instruction to interface with the
 * accelerator, and the accelerator stream contains end token to indicate it has
 * reached the end of one invoke and switch back to GPP.
 */

#include "AbstractDataFlowAcceleratorPass.h"

#define DEBUG_TYPE "AbstractDataFlowAcceleratorPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

void AbstractDataFlowAcceleratorPass::getAnalysisUsage(
    llvm::AnalysisUsage &Info) const {
  ReplayTrace::getAnalysisUsage(Info);
  Info.addRequired<llvm::PostDominatorTreeWrapperPass>();
}

bool AbstractDataFlowAcceleratorPass::initialize(llvm::Module &Module) {
  // Call the base initialization.
  bool Ret = ReplayTrace::initialize(Module);

  // Clear the stats.
  this->Stats.clear();

  this->DataFlowFileName = this->OutTraceName + ".adfa";
  auto SlashPos = this->DataFlowFileName.rfind('/');
  if (SlashPos == std::string::npos) {
    this->RelativeDataFlowFileName = this->DataFlowFileName;
  } else {
    this->RelativeDataFlowFileName = this->DataFlowFileName.substr(SlashPos);
  }

  // If the main datagraph use text mode, we also use text mode for ours.
  this->DataFlowSerializer = new TDGSerializer(this->DataFlowFileName,
                                               GemForgeOutputDataGraphTextMode);
  // Reset other variables.
  this->State = SEARCHING;
  // A window of one thousand instructions is profitable for dataflow?
  this->BufferThreshold = 50;

  this->MemorizedLoopDataflow.clear();

  // Always remember to initialize the static information.
  // Here I use the same as the main inst stream.
  this->DataFlowSerializer->serializeStaticInfo(this->StaticInfo);
  return Ret;
}

bool AbstractDataFlowAcceleratorPass::finalize(llvm::Module &Module) {
  this->MemorizedLoopDataflow.clear();

  // Release the data flow serializer.
  delete this->DataFlowSerializer;
  this->DataFlowSerializer = nullptr;
  return ReplayTrace::finalize(Module);
}

void AbstractDataFlowAcceleratorPass::transform() {
  /**
   * A state machine.
   * SEARCHING:
   *    Initial state. Searching for the start of an inner most loop
   * iteration. If encounter an inner most loop iteration, switch to
   * buffering. Otherwise, commit this instruction.
   *
   * BUFFERING:
   *    Buffer until some number of multiple of iterations and the number of
   * instructions reaches a threshold (longer enough to amortize the overhead of
   * switching to dataflow?).
   *    If it ever reaches this long, we look into the buffered instructions and
   * check some characteristics, e.g. control divergence. If it seems
   * profitable, we will insert configure instruction into the stream and
   * serialize the datagraph to another file for simplicity. And switch to
   * DATAFLOW state.
   *    If not, we just fall back to SEARCHING.
   *
   * DATAFLOW:
   *    In this state, it means we have already configured the dataflow
   * accelerator and we just keep serializing the datagraph to another file
   * until we breaks out the current loop.
   *    Depending on the new breaking instruction, we will either switch to
   * SEARCHING or BUFFERING for the next candidate.
   */
  this->State = SEARCHING;
  uint64_t Count = 0;

  // Varaibles valid only for BUFFERING state.
  llvm::Loop *CurrentLoop;
  uint32_t LoopIter;

  LLVM_DEBUG(llvm::errs() << "ADFA: start.\n");

  while (true) {
    auto NewDynamicInst = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    // If we are at the header of some candidate loop, can be the same loop or
    // some other loop.
    bool IsAtHeaderOfCandidate = false;

    if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      // LLVM_DEBUG(llvm::errs() << "Loaded new instruction.\n");
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      // Handle the cache warmer.
      if (Utils::isMemAccessInst(NewStaticInst)) {
        this->CacheWarmerPtr->addAccess(*NewDynamicInst,
                                        this->Trace->DataLayout);
      }

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        if (this->isLoopDataFlow(NewLoop)) {
          IsAtHeaderOfCandidate =
              LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);

          // Allocate and update the stats.
          {
            auto StatLoop = NewLoop;
            while (StatLoop != nullptr) {
              if (this->Stats.find(StatLoop) == this->Stats.end()) {
                // Set the static instruction's count to 0 for now.
                this->Stats.emplace(
                    StatLoop,
                    DataFlowStats(LoopUtils::getLoopId(StatLoop),
                                  LoopUtils::getNumStaticInstInLoop(StatLoop)));
              }
              this->Stats.at(StatLoop).DynamicInst++;
              StatLoop = StatLoop->getParentLoop();
            }
          }
        } else {
          // This is not a candidate.
          NewLoop = nullptr;
        }
      }

    } else {
      // Commit any remaining buffered instructions when hitting the end.
      if (this->State == DATAFLOW) {
        // Simply send the end token to the data flow.
        this->CurrentConfiguredDataFlow.end();
        break;
      }
      // After these point, it can only be SEARCHING or BUFFERING.
      // Either case, we can simply commit all of them.
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->serializeInstStream(this->Trace->DynamicInstructionList.front());
        this->Trace->commitOneDynamicInst();
      }
      break;
    }
    // State machine.
    switch (this->State) {
    case SEARCHING: {
      assert(this->Trace->DynamicInstructionList.size() <= 2 &&
             "For searching state, there should be at most 2 dynamic "
             "instructions in the buffer.");

      if (this->Trace->DynamicInstructionList.size() == 2) {
        // We can commit the previous one.
        // LLVM_DEBUG(llvm::errs() << "ADFA: SEARCHING Serialize.\n");
        this->serializeInstStream(this->Trace->DynamicInstructionList.front());
        this->Trace->commitOneDynamicInst();

        // LLVM_DEBUG(llvm::errs() << "ADFA: SEARCHING Serialize: Done.\n");
      }

      // If we are at the head of some candidate loop, switch to BUFFERING.
      if (IsAtHeaderOfCandidate) {
        this->State = BUFFERING;
        LoopIter = 0;
        CurrentLoop = NewLoop;
        // LLVM_DEBUG(llvm::errs() << "ADFA: SEARCHING -> BUFFERING.\n");
      }

      break;
    }
    case DATAFLOW: {
      assert(this->Trace->DynamicInstructionList.size() >= 2 &&
             "For dataflow state, there should be at least 2 dynamic "
             "instructions in the buffer.");

      // Add the previous inst to data flow.
      if (this->Trace->DynamicInstructionList.size() >= 2) {
        auto Iter = this->Trace->DynamicInstructionList.end();
        --Iter;
        --Iter;
        this->CurrentConfiguredDataFlow.addDynamicInst(Iter);
        this->Stats.at(CurrentConfiguredDataFlow.Loop).DataFlowDynamicInst++;
      }

      if (!CurrentLoop->contains(NewLoop)) {
        // We are outside of the current loop.
        this->CurrentConfiguredDataFlow.end();
        if (IsAtHeaderOfCandidate) {
          // We are at the header of some new candidate loop, switch to
          // BUFFERING.
          LoopIter = 0;
          CurrentLoop = NewLoop;
          this->State = BUFFERING;
          // LLVM_DEBUG(llvm::errs() << "ADFA: DATAFLOW -> BUFFERING.\n");
        } else {
          // We are back to search state.
          this->State = SEARCHING;
          // LLVM_DEBUG(llvm::errs() << "ADFA: DATAFLOW -> SEARCHING.\n");
        }
      }

      break;
    }
    case BUFFERING: {
      /**
       * Update the LoopIter.
       * 1. If we are inside other loops, increase iter count.
       * 2. If we are in the same loop, but hitting header, increase iter
       * count.
       */
      bool IsAtBoundary = false;
      if (!CurrentLoop->contains(NewLoop)) {
        // We are out of the current loop.
        LoopIter++;
        IsAtBoundary = true;
      } else {
        // We are still in the same loop.
        if (CurrentLoop == NewLoop && IsAtHeaderOfCandidate) {
          // We are back at the header of the current loop.
          LoopIter++;
          IsAtBoundary = true;
        }
      }

      if (!IsAtBoundary) {
        // Keep buffering if we are not at boundary.
        break;
      }

      /**
       * Process the buffer if we buffered enough number of instructions.
       * Be careful to only process the buffer at boundary (one iter ends).
       * This will commit the buffer EXCEPT THE LAST INSTRUCTION.
       */
      bool DataFlowStarted = false;
      if (this->Trace->DynamicInstructionList.size() > this->BufferThreshold) {
        // LLVM_DEBUG(llvm::errs() << "ADFA: Processing buffer.\n");
        DataFlowStarted = this->processBuffer(CurrentLoop, LoopIter);
        // Clear the loop iter.
        LoopIter = 0;
      }

      /**
       * Determine next state.
       * 1. If we are staying in the same loop,
       *  1.1 If the data flow has already started, then switch to DATAFLOW.
       *  2.2 Otherwise, keep buffering. (Not sure about this).
       * 2. If we are outside the current loop,
       *  2.1 If we are at the header of a new candidate, keep buffering the
       * new loop. 2.2. Otherwise, go back to search. In case 2, we should
       * also commit remaining buffered iters in case the buffered number of
       * instructions is not big enough.
       */
      if (!CurrentLoop->contains(NewLoop)) {
        // Case 2
        if (IsAtHeaderOfCandidate) {
          // Case 2.1.
          // Update the loop and loop iter.
          LoopIter = 0;
          CurrentLoop = NewLoop;
        } else {
          // Case 2.2.
          // LLVM_DEBUG(llvm::errs() << "ADFA: BUFFERING -> SEARCHING.\n");
          this->State = SEARCHING;
        }

        if (DataFlowStarted) {
          // We are out of the current loop, end the dataflow.
          this->CurrentConfiguredDataFlow.end();
          assert(this->Trace->DynamicInstructionList.size() == 1 &&
                 "Data flow ended with remaining instructions in the buffer.");
        } else {
          // Commit remaining insts if we haven't start the dataflow.
          while (this->Trace->DynamicInstructionList.size() > 1) {
            this->serializeInstStream(
                this->Trace->DynamicInstructionList.front());
            this->Trace->commitOneDynamicInst();
          }
        }

      } else {
        // Case 1.
        if (DataFlowStarted) {
          // LLVM_DEBUG(llvm::errs() << "ADFA: BUFFERING -> DATAFLOW.\n");
          this->State = DATAFLOW;
        }
      }

      break;
    }
    default: {
      llvm_unreachable("ADFA: Invalid machine state.");
      break;
    }
    }
  }

  LLVM_DEBUG(llvm::errs() << "ADFA end.\n");
}

void AbstractDataFlowAcceleratorPass::serializeInstStream(
    DynamicInstruction *DynamicInst) {
  this->Serializer->serialize(DynamicInst, this->Trace);
}

void AbstractDataFlowAcceleratorPass::serializeDataFlow(
    DynamicInstruction *DynamicInst) {
  this->DataFlowSerializer->serialize(DynamicInst, this->Trace);
}

/**
 * I know this this confusing, but this is not an real instruction, but just
 * tell the accelerator that it has reached the end of this invoke.
 */
class AbsDataFlowEndToken : public DynamicInstruction {
public:
  AbsDataFlowEndToken() {}
  std::string getOpName() const override { return "df-end"; }
  // No customized fields for AbsDataFlowEndToken.
};

/**
 * This instruction will configure the dataflow accelerator.
 * Involves some overhead.
 */
class AbsDataFlowConfigInst : public DynamicInstruction {
public:
  AbsDataFlowConfigInst(const std::string &_DataFlowFileName, uint64_t _StartPC,
                        const std::string &_RegionName)
      : DataFlowFileName(_DataFlowFileName), StartPC(_StartPC),
        RegionName(_RegionName) {}
  std::string getOpName() const override { return "df-config"; }
  // There should be some customized fields in the future.
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    // Calling mutable_adfa_config should set this field.
    auto ConfigExtra = ProtobufEntry->mutable_adfa_config();
    assert(ProtobufEntry->has_adfa_config() &&
           "The protobuf entry should have adfa config extra struct.");
    ConfigExtra->set_data_flow(this->DataFlowFileName);
    ConfigExtra->set_start_pc(this->StartPC);
    ConfigExtra->set_region(this->RegionName);
  }

private:
  std::string DataFlowFileName;
  uint64_t StartPC;
  std::string RegionName;
};

/**
 * This instruction will kick the accelerator to start working.
 */
class AbsDataFlowStartInst : public DynamicInstruction {
public:
  AbsDataFlowStartInst() {}
  std::string getOpName() const override { return "df-start"; }
};

AbsDataFlowLLVMInst::~AbsDataFlowLLVMInst() {
  delete this->LLVMDynInst;
  this->LLVMDynInst = nullptr;
}

void AbsDataFlowLLVMInst::serializeToProtobufExtra(
    LLVM::TDG::TDGInstruction *ProtobufEntry, DataGraph *DG) const {
  /**
   * Serialize our extra dependence information.
   */
  for (const auto &Id : this->UnrollableCtrDeps) {
    auto Dep = ProtobufEntry->add_deps();
    Dep->set_type(::LLVM::TDG::TDGInstructionDependence::UNROLLABLE_CONTROL);
    Dep->set_dependent_id(Id);
  }
  for (const auto &Id : this->PDFCtrDeps) {
    auto Dep = ProtobufEntry->add_deps();
    Dep->set_type(
        ::LLVM::TDG::TDGInstructionDependence::POST_DOMINANCE_FRONTIER);
    Dep->set_dependent_id(Id);
  }
  for (const auto &Id : this->IVDeps) {
    auto Dep = ProtobufEntry->add_deps();
    Dep->set_type(::LLVM::TDG::TDGInstructionDependence::INDUCTION_VARIABLE);
    Dep->set_dependent_id(Id);
  }
  for (const auto &Id : this->RVDeps) {
    auto Dep = ProtobufEntry->add_deps();
    Dep->set_type(::LLVM::TDG::TDGInstructionDependence::REDUCTION_VARIABLE);
    Dep->set_dependent_id(Id);
  }

  this->LLVMDynInst->serializeToProtobufExtra(ProtobufEntry, DG);
}

bool AbstractDataFlowAcceleratorPass::processBuffer(llvm::Loop *Loop,
                                                    uint32_t LoopIter) {
  // Simply serialize to data flow stream for now.
  if (true) {
    /**
     * Note that all the config/start/end insts will be handled as serialization
     * points in the datagraph simulator, so no need to add dependence
     * information.
     */
    // Check if we need to configure the accelerator.
    if (Loop != this->CurrentConfiguredDataFlow.Loop) {
      // Get the start pc of this loop (use the pointer directly as the pc).
      auto StartInst =
          this->Trace->DynamicInstructionList.front()->getStaticInstruction();
      assert(StartInst != nullptr &&
             "Failed to get the start instruction for this loop.");
      assert(Loop->contains(StartInst->getParent()) &&
             "The start instruction should be in the loop.");

      AbsDataFlowConfigInst ConfigInst(this->RelativeDataFlowFileName,
                                       reinterpret_cast<uint64_t>(StartInst),
                                       LoopUtils::getLoopId(Loop));
      this->serializeInstStream(&ConfigInst);
      LLVM_DEBUG(llvm::errs() << "ADFA: configure the accelerator to loop "
                              << printLoop(Loop) << '\n');
      auto Func = Loop->getHeader()->getParent();
      // Configure the dynamic data flow.
      this->CurrentConfiguredDataFlow.configure(
          Loop, this->CachedLI->getLoopInfo(Func),
          this->CachedPDF->getPostDominanceFrontier(Func), this->Trace,
          this->DataFlowSerializer, this->CachedLU,
          this->CachedLI->getScalarEvolution(Func));
      this->Stats.at(Loop).Config++;
    }

    // Serialize the start inst.
    {
      LLVM_DEBUG(llvm::errs() << "ADFA: start the accelerator to loop "
                              << printLoop(Loop) << '\n');
      AbsDataFlowStartInst StartInst;
      this->serializeInstStream(&StartInst);
      // Start the dynamic data flow.
      this->CurrentConfiguredDataFlow.start();
      this->Stats.at(Loop).DataFlow++;
    }

    // Add the instruction to the dataflow buffer.
    auto End = this->Trace->DynamicInstructionList.end();
    --End;
    auto Iter = this->Trace->DynamicInstructionList.begin();
    while (Iter != End) {
      auto Next = Iter;
      ++Next;
      this->CurrentConfiguredDataFlow.addDynamicInst(Iter);
      this->Stats.at(Loop).DataFlowDynamicInst++;
      Iter = Next;
    }
    return true;
  }

  while (this->Trace->DynamicInstructionList.size() > 1) {
    this->serializeInstStream(this->Trace->DynamicInstructionList.front());
    this->Trace->commitOneDynamicInst();
  }

  return false;
}

bool AbstractDataFlowAcceleratorPass::isLoopDataFlow(llvm::Loop *Loop) {
  assert(Loop != nullptr && "Loop should not be nullptr.");

  auto Iter = this->MemorizedLoopDataflow.find(Loop);
  if (Iter != this->MemorizedLoopDataflow.end()) {
    return Iter->second;
  }

  // We allow nested loops.
  bool IsDataFlow = true;
  if (!LoopUtils::isLoopContinuous(Loop)) {
    // LLVM_DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
    //                    << " is not dataflow as it is not continuous.\n");

    IsDataFlow = false;
  }

  // Done: this loop can be represented as data flow.
  // LLVM_DEBUG(llvm::errs() << "isLoopDataFlow returned true.\n");
  this->MemorizedLoopDataflow.emplace(Loop, IsDataFlow);
  return IsDataFlow;
}

void AbstractDataFlowAcceleratorPass::dumpStats(std::ostream &O) {
  O << "----------- ADFA ------------\n";
  DataFlowStats::dumpStatsTitle(O);
  // Sort the stats with according to their dynamic insts.
  std::multimap<uint64_t, const DataFlowStats *> SortedStats;
  for (const auto &Entry : this->Stats) {
    SortedStats.emplace(Entry.second.DynamicInst, &Entry.second);
  }
  for (auto StatIter = SortedStats.rbegin(), StatEnd = SortedStats.rend();
       StatIter != StatEnd; ++StatIter) {
    StatIter->second->dumpStats(O);
  }
}

DynamicDataFlow::DynamicDataFlow()
    : Loop(nullptr), PDF(nullptr), CurrentAge(0) {}

void DynamicDataFlow::configure(llvm::Loop *_Loop, llvm::LoopInfo *_LI,
                                const PostDominanceFrontier *_PDF,
                                DataGraph *_DG, TDGSerializer *_Serializer,
                                CachedLoopUnroller *_CachedLU,
                                llvm::ScalarEvolution *_SE) {
  this->Loop = _Loop;
  this->LI = _LI;
  this->PDF = _PDF;
  this->DG = _DG;
  this->Serializer = _Serializer;
  this->CachedLU = _CachedLU;
  this->SE = _SE;
  this->CurrentAge = 0;
  LLVM_DEBUG(llvm::errs() << "Config the dynamic data flow for loop "
                          << this->Loop->getName() << '\n');
  LLVM_DEBUG(llvm::errs() << "Post Dominance Frontier:\n");
  LLVM_DEBUG(this->PDF->print(llvm::errs()));
}

void DynamicDataFlow::fixCtrDependence(AbsDataFlowLLVMInst *AbsDFInst) {
  auto StaticInst = AbsDFInst->getStaticInstruction();
  assert(StaticInst != nullptr &&
         "DynamicDataFlow cannot fix dependence for non llvm instructions.");

  /**
   * Notice that we remove the original control dependence.
   */
  auto &CtrDeps = this->DG->CtrDeps.at(AbsDFInst->getId());
  CtrDeps.clear();

  auto CtrDepId = DynamicInstruction::InvalidId;
  llvm::Instruction *CtrDepStaticInst = nullptr;
  uint64_t CtrDepAge;
  for (auto CtrBB : this->PDF->getFrontier(StaticInst->getParent())) {
    // Get the terminator of the CtrBB.
    auto CtrInst = CtrBB->getTerminator();
    auto Iter = this->StaticToDynamicMap.find(CtrInst);
    if (Iter == this->StaticToDynamicMap.end()) {
      continue;
    }
    if (CtrDepId == DynamicInstruction::InvalidId) {
      // The first control dependence we have met.
      CtrDepAge = Iter->second.second;
      CtrDepId = Iter->second.first;
      CtrDepStaticInst = CtrInst;
    } else if (CtrDepAge < Iter->second.second) {
      // Later control dependence.
      CtrDepAge = Iter->second.second;
      CtrDepId = Iter->second.first;
      CtrDepStaticInst = CtrInst;
    }
  }
  // Update the ctr dep.
  if (CtrDepId != DynamicInstruction::InvalidId) {

    // Check if this is an unrollable back deps.
    auto LU = this->CachedLU->getUnroller(
        this->LI->getLoopFor(StaticInst->getParent()), this->SE);
    if (CtrDepStaticInst == LU->getUnrollableTerminator()) {
      // This is an unrollable terminator dependence.
      AbsDFInst->UnrollableCtrDeps.insert(CtrDepId);
    } else {
      // Normal PDF dependence.
      AbsDFInst->PDFCtrDeps.insert(CtrDepId);
    }
  }
}

void DynamicDataFlow::fixRegDependence(AbsDataFlowLLVMInst *AbsDFInst) {
  auto LU = this->CachedLU->getUnroller(
      this->LI->getLoopFor(AbsDFInst->getStaticInstruction()->getParent()),
      this->SE);

  auto &RegDeps = this->DG->RegDeps.at(AbsDFInst->getId());
  for (auto RegDepIter = RegDeps.begin(), RegDepEnd = RegDeps.end();
       RegDepIter != RegDepEnd;) {
    auto StaticOperand = RegDepIter->first;
    if (StaticOperand == nullptr) {
      // Ingore those non-llvm dependence.
      ++RegDepIter;
      continue;
    }
    LLVM_DEBUG(llvm::errs()
               << "Reg dependent on inst " << StaticOperand->getName() << '\n');
    if (LU->isInductionVariable(StaticOperand)) {
      // This is an induction variable.
      LLVM_DEBUG(llvm::errs() << "Find IV dependence on IV "
                              << StaticOperand->getName() << '\n');
      AbsDFInst->IVDeps.insert(RegDepIter->second);
      RegDepIter = RegDeps.erase(RegDepIter);
      continue;
    }
    if (LU->isReductionVariable(StaticOperand)) {
      // This is an reduction variable.
      AbsDFInst->RVDeps.insert(RegDepIter->second);
      RegDepIter = RegDeps.erase(RegDepIter);
      continue;
    }
    // Update the iterator to the next one.
    ++RegDepIter;
  }
}

void DynamicDataFlow::updateStaticToDynamicMap(
    DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr &&
         "DynamicDataFlow cannot handle non llvm instructions.");

  auto Iter = this->StaticToDynamicMap.find(StaticInst);
  if (Iter == this->StaticToDynamicMap.end()) {
    this->StaticToDynamicMap.emplace(
        std::piecewise_construct, std::forward_as_tuple(StaticInst),
        std::forward_as_tuple(DynamicInst->getId(), this->CurrentAge++));
  } else {
    Iter->second.first = DynamicInst->getId();
    Iter->second.second = this->CurrentAge++;
  }
}

AbsDataFlowLLVMInst *DynamicDataFlow::wrapWithAbsDataFlowLLVMInst(
    DataGraph::DynamicInstIter &DynInstIter) const {
  auto DynInst = *DynInstIter;
  assert(DynInst->getStaticInstruction() != nullptr &&
         "DynamicDataFlow cannot handle non llvm instructions.");
  auto LLVMDynInst = reinterpret_cast<LLVMDynamicInstruction *>(DynInst);
  auto WrapDynInst = new AbsDataFlowLLVMInst(LLVMDynInst);
  *DynInstIter = WrapDynInst;
  return WrapDynInst;
}

void DynamicDataFlow::addDynamicInst(DataGraph::DynamicInstIter DynInstIter) {
  auto AbsDFInst = this->wrapWithAbsDataFlowLLVMInst(DynInstIter);
  LLVM_DEBUG(llvm::errs() << "ADFA: Add dynamic instruction to the data flow "
                          << AbsDFInst->getOpName() << '\n');

  // First fix the control dependence.
  this->fixCtrDependence(AbsDFInst);

  // Also fix the register dependence.
  this->fixRegDependence(AbsDFInst);

  // Update our static to dynamic map.
  this->updateStaticToDynamicMap(AbsDFInst);

  // Commit the dynamic instruction.
  this->Serializer->serialize(AbsDFInst, this->DG);
  // Release the instructions.
  this->DG->commitOneDynamicInst();
}

void DynamicDataFlow::start() {
  this->StaticToDynamicMap.clear();
  this->CurrentAge = 0;
}

void DynamicDataFlow::end() {
  // We need to end the dataflow.
  LLVM_DEBUG(llvm::errs() << "ADFA: End the data flow.\n");
  LLVM_DEBUG(llvm::errs() << "ADFA::DF: Serialize end token to data flow.");
  AbsDataFlowEndToken EndToken;
  this->Serializer->serialize(&EndToken, this->DG);
}

#undef DEBUG_TYPE

char AbstractDataFlowAcceleratorPass::ID = 0;
static llvm::RegisterPass<AbstractDataFlowAcceleratorPass>
    X("abs-data-flow-acc-pass", "Abstract datagraph accelerator transform pass",
      false, false);
