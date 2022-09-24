#include "StaticStreamRegionAnalyzer.h"

#include "Utils.h"

#include "llvm/IR/IntrinsicsX86.h" // For some x86 intrinsic ids.
#include "llvm/Support/FileSystem.h"

#include <iomanip>
#include <sstream>
#include <stack>

#define DEBUG_TYPE "StaticStreamRegionAnalyzer"

StaticStreamRegionAnalyzer::StaticStreamRegionAnalyzer(
    llvm::Loop *_TopLoop, llvm::DataLayout *_DataLayout,
    CachedLoopInfo *_CachedLI, CachedPostDominanceFrontier *_CachedPDF,
    CachedBBBranchDataGraph *_CachedBBBranchDG, uint64_t _RegionIdx,
    const std::string &_RootPath)
    : TopLoop(_TopLoop), DataLayout(_DataLayout), CachedLI(_CachedLI),
      CachedBBBranchDG(_CachedBBBranchDG),
      LI(_CachedLI->getLoopInfo(_TopLoop->getHeader()->getParent())),
      SE(_CachedLI->getScalarEvolution(_TopLoop->getHeader()->getParent())),
      PDT(_CachedPDF->getPostDominatorTree(_TopLoop->getHeader()->getParent())),
      RegionIdx(_RegionIdx), RootPath(_RootPath) {
  LLVM_DEBUG(llvm::dbgs() << "Constructing StaticStreamRegionAnalyzer for loop "
                          << LoopUtils::getLoopId(this->TopLoop) << '\n');

  // Initialize the folder for this region.
  std::stringstream ss;
  ss << "R." << this->RegionIdx << ".A." << LoopUtils::getLoopId(this->TopLoop);
  this->AnalyzeRelativePath = ss.str();
  this->AnalyzePath = this->RootPath + "/" + this->AnalyzeRelativePath;

  {
    auto ErrCode = llvm::sys::fs::exists(this->AnalyzePath);
    assert(!ErrCode && "AnalyzePath already exists.");
  }

  auto ErrCode = llvm::sys::fs::create_directory(this->AnalyzePath);
  if (ErrCode) {
    llvm::errs() << "Failed to create AnalyzePath: " << this->AnalyzePath
                 << ". Reason: " << ErrCode.message() << '\n';
  }
  assert(!ErrCode && "Failed to create AnalyzePath.");

  this->initializeStreams();
  LLVM_DEBUG(llvm::dbgs() << "Initializing streams done.\n");
  this->markPredicateRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark predicate relationship done.\n");
  this->buildStreamAddrDepGraph();
  LLVM_DEBUG(llvm::dbgs() << "Build StreamAddrDepGraph done.\n");
  /**
   * After building AddrDepGraph, IVStream should have constructed MetaGraph and
   * analyzed ValPattern.
   * Now we collect ReductionFinalInst and mark them as PlaceholderInst for
   * the ReduceStream in the InstStaticStreamMap.
   * This information is used in buildStreamValueDepGraph() to correctly
   * recognize UserStream of the ReduceStream.
   */
  this->collectReduceFinalInsts();
  LLVM_DEBUG(llvm::dbgs() << "Collect ReduceFinalInsts done.\n");
  this->markAliasRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark alias relationship done.\n");
  /**
   * After marking Alias relationship, we try to fuse load ops. This is done
   * here because we want to use a heuristic to fuse load ops using multiple
   * load streams with small AliasOffset.
   *
   * Similar to FinalReduceResult, the FinalInst now points to the LoadStream.
   */
  this->fuseLoadOps();
  LLVM_DEBUG(llvm::dbgs() << "Fuse LoadOps done.\n");
  this->buildStreamValueDepGraph();
  LLVM_DEBUG(llvm::dbgs() << "Build StreamValueDepGraph done.\n");
  /**
   * Update relationship must be after alias relationship.
   */
  this->markUpdateRelationship();
  LLVM_DEBUG(llvm::dbgs() << "Mark update relationship done.\n");
  this->analyzeIsCandidate();
  LLVM_DEBUG(llvm::dbgs() << "AnalyzeIsCandidate done.\n");
  this->markQualifiedStreams();
  LLVM_DEBUG(llvm::dbgs() << "Marking qualified streams done.\n");
  this->enforceBackEdgeDependence();
  LLVM_DEBUG(llvm::dbgs() << "Enforce back-edge dependence done.\n");
  LLVM_DEBUG(llvm::dbgs()
             << "Constructing StaticStreamRegionAnalyzer for loop done!\n");
}

StaticStreamRegionAnalyzer::~StaticStreamRegionAnalyzer() {
  this->InstChosenStreamMap.clear();
  // First clear all PlaceHolder Inst.
  this->FinalInstStaticStreamMap.clear();
  // Now clear real streams.
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
      if (Utils::isMemAccessInst(StaticInst) ||
          UserDefinedMemStream::isUserDefinedMemStream(StaticInst)) {
        this->initializeStreamForAllLoops(StaticInst);
      }
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
    StaticStream *NewStream = nullptr;
    LLVM_DEBUG(llvm::dbgs()
               << "Initializing stream " << Utils::formatLLVMInst(StreamInst)
               << " Config Loop " << LoopUtils::getLoopId(ConfigureLoop)
               << '\n');
    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(StreamInst)) {
      NewStream = new StaticIndVarStream(PHIInst, ConfigureLoop, InnerMostLoop,
                                         SE, PDT, DataLayout);
    } else if (UserDefinedMemStream::isUserDefinedMemStream(StreamInst)) {
      NewStream = new UserDefinedMemStream(StreamInst, ConfigureLoop,
                                           InnerMostLoop, SE, PDT, DataLayout);
    } else {
      NewStream = new StaticMemStream(StreamInst, ConfigureLoop, InnerMostLoop,
                                      SE, PDT, DataLayout);
    }
    Streams.emplace_back(NewStream);

    ConfigureLoop = ConfigureLoop->getParentLoop();
  } while (this->TopLoop->contains(ConfigureLoop));
}

StaticStream *StaticStreamRegionAnalyzer::getStreamInMapByInstAndConfigureLoop(
    const InstStaticStreamMapT &Map, const llvm::Instruction *Inst,
    const llvm::Loop *ConfigureLoop) const {
  auto Iter = Map.find(Inst);
  if (Iter == Map.end()) {
    return nullptr;
  }
  const auto &Streams = Iter->second;
  for (const auto &S : Streams) {
    if (S->ConfigureLoop == ConfigureLoop) {
      return S;
    }
  }
  return nullptr;
}

StaticStream *StaticStreamRegionAnalyzer::getStreamByInstAndConfigureLoop(
    const llvm::Instruction *Inst, const llvm::Loop *ConfigureLoop) const {
  return this->getStreamInMapByInstAndConfigureLoop(this->InstStaticStreamMap,
                                                    Inst, ConfigureLoop);
}

StaticStream *StaticStreamRegionAnalyzer::getStreamWithPlaceholderInst(
    const llvm::Instruction *Inst, const llvm::Loop *ConfigureLoop) const {
  if (auto S = this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop)) {
    return S;
  }
  // Check in the Placeholder map.
  return this->getStreamInMapByInstAndConfigureLoop(
      this->FinalInstStaticStreamMap, Inst, ConfigureLoop);
}

void StaticStreamRegionAnalyzer::markUpdateRelationship() {
  /**
   * The idea is to update a load to update stream.
   */
  if (!StreamPassUpgradeLoadToUpdate) {
    return;
  }

  /**
   * Search for load-store update relationship.
   */
  for (auto &InstStreams : this->InstStaticStreamMap) {
    auto Inst = InstStreams.first;
    if (Inst->getOpcode() == llvm::Instruction::Load) {
      for (auto LoadSS : this->InstStaticStreamMap.at(Inst)) {
        this->markUpdateRelationshipForLoadStream(LoadSS);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::markUpdateRelationshipForLoadStream(
    StaticStream *LoadSS) {
  /**
   * Update relationship is a special case for value dependence.
   * 1. The StoreStream stores to the same address of the LoadStream, with same
   * type.
   * 2. There is only one such StoreStream.
   * 3. They are in the same BB.
   * 4. The LoadSS has fixed trip count and unconditional access.
   * TODO: We should check that there is no aliasing store between them.
   */
  LLVM_DEBUG(llvm::dbgs() << "== SSRA: Mark Update Relationship for "
                          << LoadSS->getStreamName() << '\n');
  // The current CondAccess analysis is very conservative.
  // It basically complains all stream configured at outer loop is conditional
  // access.
  // ! Here I disable it.
  // if (LoadSS->StaticStreamInfo.is_cond_access())
  // {
  //   LLVM_DEBUG(llvm::dbgs() << "[No Update] ConditalAccess "
  //                           << LoadSS->getStreamName() << '\n');
  //   return;
  // }
  if (!LoadSS->StaticStreamInfo.is_trip_count_fixed()) {
    LLVM_DEBUG(llvm::dbgs() << "[No Update] VariableTripCount "
                            << LoadSS->getStreamName() << '\n');
    return;
  }

  auto LoadType = Utils::getMemElementType(LoadSS->Inst);
  auto AliasBaseSS = LoadSS->AliasBaseStream;
  if (!AliasBaseSS) {
    return;
  }
  StaticStream *CandidateSS = nullptr;
  for (auto AliasSS : AliasBaseSS->AliasedStreams) {
    LLVM_DEBUG(llvm::dbgs() << "==== Check AliasStream "
                            << AliasSS->getStreamName() << '\n');
    if (AliasSS->AliasOffset != LoadSS->AliasOffset) {
      continue;
    }
    if (!Utils::isStoreInst(AliasSS->Inst)) {
      continue;
    }
    if (Utils::getMemElementType(AliasSS->Inst) != LoadType) {
      continue;
    }
    if (CandidateSS) {
      // Multiple candidates.
      return;
    } else {
      LLVM_DEBUG(llvm::dbgs() << "==== Select candidate "
                              << AliasSS->getStreamName() << '\n');
      CandidateSS = AliasSS;
    }
  }

  if (!CandidateSS) {
    return;
  }

  if (CandidateSS->Inst->getParent() != LoadSS->Inst->getParent()) {
    // Not same BB.
    return;
  }

  // Make sure we don't have multiple update relationship for same StoreSS.
  if (CandidateSS->UpdateStream) {
    return;
  }
  assert(!LoadSS->UpdateStream && "This LoadSS already has UpdateStream.");
  LLVM_DEBUG(llvm::dbgs() << "==== Set Update Stream "
                          << CandidateSS->getStreamName() << '\n');
  LoadSS->UpdateStream = CandidateSS;
  CandidateSS->UpdateStream = LoadSS;
}

void StaticStreamRegionAnalyzer::markPredicateRelationship() {
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {
      this->markPredicateRelationshipForLoopBB(Loop, BB);
    }
  }
}

void StaticStreamRegionAnalyzer::markPredicateRelationshipForLoopBB(
    const llvm::Loop *Loop, const llvm::BasicBlock *BB) {
  auto BBPredDG = this->CachedBBBranchDG->getBBBranchDataGraph(Loop, BB);
  if (!BBPredDG->isValidPredicate()) {
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid " << BB->getName() << '\n');
    return;
  }
  auto PredInputLoads = BBPredDG->getInputLoads();
  if (PredInputLoads.size() != 1) {
    // Complicate predicate is not supported.
    LLVM_DEBUG(llvm::dbgs()
               << "BBPredDG Multiple Load Inputs " << BB->getName() << '\n');
    return;
  }
  auto PredLoadInst = *(PredInputLoads.begin());
  auto PredLoadStream =
      this->getStreamByInstAndConfigureLoop(PredLoadInst, Loop);
  assert(PredLoadStream->BBPredDG == nullptr && "Multiple BBPredDG.");
  PredLoadStream->BBPredDG = BBPredDG;

  auto MarkStreamInBB = [this, PredLoadStream](const llvm::BasicBlock *TargetBB,
                                               bool PredicatedTrue) -> void {
    auto ConfigureLoop = PredLoadStream->ConfigureLoop;
    for (const auto &TargetInst : *TargetBB) {
      auto TargetStream =
          this->getStreamByInstAndConfigureLoop(&TargetInst, ConfigureLoop);
      if (!TargetStream) {
        continue;
      }
      LLVM_DEBUG(llvm::dbgs()
                 << "Add predicated relation "
                 << PredLoadStream->getStreamName() << " -- " << PredicatedTrue
                 << " --> " << TargetStream->getStreamName() << '\n');
      if (PredicatedTrue) {
        PredLoadStream->PredicatedTrueStreams.insert(TargetStream);
      } else {
        PredLoadStream->PredicatedFalseStreams.insert(TargetStream);
      }
    }
  };
  if (auto TrueBB = BBPredDG->getPredicateBB(true)) {
    MarkStreamInBB(TrueBB, true);
  }
  if (auto FalseBB = BBPredDG->getPredicateBB(false)) {
    MarkStreamInBB(FalseBB, false);
  }
}

void StaticStreamRegionAnalyzer::insertPredicateFuncInModule(
    std::unique_ptr<llvm::Module> &Module) {
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {

      auto BBPredDG = this->CachedBBBranchDG->tryBBBranchDataGraph(Loop, BB);
      if (!BBPredDG) {
        continue;
      }
      if (!BBPredDG->isValid()) {
        continue;
      }
      BBPredDG->generateFunction(BBPredDG->getFuncName(), Module);
    }
  }
}

void StaticStreamRegionAnalyzer::insertAddrFuncInModule(
    std::unique_ptr<llvm::Module> &Module) {
  for (auto &InstStream : this->InstChosenStreamMap) {
    auto Inst = InstStream.first;
    auto S = InstStream.second;
    S->generateReduceFunction(Module);
    S->generateValueFunction(Module);
    S->generateAddrFunction(Module);
  }
}

void StaticStreamRegionAnalyzer::buildStreamAddrDepGraph() {
  auto GetStream = [this](const llvm::Instruction *Inst,
                          const llvm::Loop *ConfigureLoop) -> StaticStream * {
    return this->getStreamByInstAndConfigureLoop(Inst, ConfigureLoop);
  };
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      LLVM_DEBUG(llvm::dbgs() << "== SSRA: Construct Graph for "
                              << S->getStreamName() << '\n');
      S->constructGraph(GetStream);
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: constructGraph done.\n");
  /**
   * After add all the base streams, we are going to compute the base step root
   * streams. The computeBaseStepRootStreams() is by itself recursive. This will
   * result in some overhead, but hopefully the dependency chain is not very
   * long.
   */
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      LLVM_DEBUG(llvm::dbgs() << "== SSRA: ComputeBaseStepRoot for "
                              << S->getStreamName() << '\n');
      S->computeBaseStepRootStreams();
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: computeBaseStepRootStreams done.\n");
}

void StaticStreamRegionAnalyzer::collectReduceFinalInsts() {

  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      if (S->ReduceDG) {
        auto ReduceFinalInst = llvm::dyn_cast<llvm::Instruction>(
            S->ReduceDG->getSingleResultValue());
        assert(ReduceFinalInst && "ReduceFinalValue is not Instruction.");
        LLVM_DEBUG(llvm::dbgs()
                   << "== SSRA: Collect ReduceInst "
                   << Utils::formatLLVMInstWithoutFunc(ReduceFinalInst)
                   << " from " << S->getStreamName() << '\n');

        this->FinalInstStaticStreamMap
            .emplace(std::piecewise_construct,
                     std::forward_as_tuple(ReduceFinalInst),
                     std::forward_as_tuple())
            .first->second.push_back(S);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::markAliasRelationship() {
  /**
   * After construct basic stream dependence graph, we can analyze the coalesce
   * relationship. This is used when building LoadStreamDependenceGraph.
   */
  std::unordered_set<const llvm::Loop *> VisitedLoops;
  for (auto BBIter = this->TopLoop->block_begin(),
            BBEnd = this->TopLoop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    // Search for each loop.
    for (auto Loop = this->LI->getLoopFor(BB); this->TopLoop->contains(Loop);
         Loop = Loop->getParentLoop()) {
      if (!VisitedLoops.count(Loop)) {
        this->markAliasRelationshipForLoopBB(Loop);
        VisitedLoops.insert(Loop);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::markAliasRelationshipForLoopBB(
    const llvm::Loop *Loop) {
  /**
   * We mark aliased memory streams if:
   * 1. They share the same step root.
   * 2. They have scev.
   * 3. Their SCEV has a constant offset.
   */
  using StreamOffsetPair = std::pair<StaticStream *, int64_t>;
  std::list<std::vector<StreamOffsetPair>> CoalescedGroup;
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      auto S = this->getStreamByInstAndConfigureLoop(Inst, Loop);
      if (!S) {
        continue;
      }
      if (S->Type == StaticStream::TypeT::IV ||
          S->BaseStepRootStreams.size() != 1) {
        LLVM_DEBUG(llvm::dbgs() << "====== Skip coalesce stream: "
                                << S->getStreamName() << " BaseStepRoots "
                                << S->BaseStepRootStreams.size() << '\n');
        continue;
      }
      LLVM_DEBUG(llvm::dbgs() << "====== Try to coalesce stream: "
                              << S->getStreamName() << '\n');
      auto Addr = const_cast<llvm::Value *>(Utils::getMemAddrValue(S->Inst));
      auto AddrSCEV = this->SE->getSCEV(Addr);

      bool Coalesced = false;
      if (AddrSCEV) {
        LLVM_DEBUG(llvm::dbgs() << "== AddrSCEV: "; AddrSCEV->dump());
        for (auto &Group : CoalescedGroup) {
          auto TargetS = Group.front().first;
          if (TargetS->BaseStepRootStreams != S->BaseStepRootStreams) {
            // Do not share the same step root.
            continue;
          }
          auto TargetAddr =
              const_cast<llvm::Value *>(Utils::getMemAddrValue(TargetS->Inst));
          auto TargetAddrSCEV = this->SE->getSCEV(TargetAddr);
          if (!TargetAddrSCEV) {
            continue;
          }
          // Check the scev.
          LLVM_DEBUG(llvm::dbgs() << "== TargetStream: "
                                  << TargetS->getStreamName() << '\n');
          LLVM_DEBUG(llvm::dbgs() << "== TargetAddrSCEV: ";
                     TargetAddrSCEV->dump());
          auto MinusSCEV = this->SE->getMinusSCEV(AddrSCEV, TargetAddrSCEV);
          LLVM_DEBUG(llvm::dbgs() << "== MinusSCEV: "; MinusSCEV->dump());
          auto OffsetSCEV = llvm::dyn_cast<llvm::SCEVConstant>(MinusSCEV);
          if (!OffsetSCEV) {
            // Not constant offset.
            continue;
          }
          LLVM_DEBUG(llvm::dbgs() << "== OffsetSCEV: "; OffsetSCEV->dump());
          int64_t Offset = OffsetSCEV->getAPInt().getSExtValue();
          LLVM_DEBUG(llvm::dbgs()
                     << "== Coalesced, offset: " << Offset
                     << " with stream: " << TargetS->getStreamName() << '\n');
          Coalesced = true;
          Group.emplace_back(S, Offset);
          break;
        }
      }

      if (!Coalesced) {
        LLVM_DEBUG(llvm::dbgs() << "== New coalesce group\n");
        CoalescedGroup.emplace_back();
        CoalescedGroup.back().emplace_back(S, 0);
      }
    }
  }

  // Sort each group with increasing order of offset.
  for (auto &Group : CoalescedGroup) {
    std::sort(Group.begin(), Group.end(),
              [](const StreamOffsetPair &a, const StreamOffsetPair &b) -> bool {
                return a.second < b.second;
              });
    // Set the base/offset.
    auto BaseS = Group.front().first;
    int64_t BaseOffset = Group.front().second;
    for (auto &StreamOffset : Group) {
      auto S = StreamOffset.first;
      auto Offset = StreamOffset.second;
      // Remember the offset.
      S->AliasBaseStream = BaseS;
      S->AliasOffset = Offset - BaseOffset;
      BaseS->AliasedStreams.push_back(S);
    }
  }
}

void StaticStreamRegionAnalyzer::fuseLoadOps() {
  if (!StreamPassEnableFuseLoadOp) {
    return;
  }
  // So far only use this for the AtomicCmpXchg/Load
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    if (Inst->getOpcode() == llvm::Instruction::AtomicCmpXchg ||
        Inst->getOpcode() == llvm::Instruction::Load) {
      for (auto S : InstStream.second) {
        this->fuseLoadOps(S);
        if (S->ValueDG) {
          // Place this into the FinalInstMap.
          this->FinalInstStaticStreamMap
              .emplace(std::piecewise_construct,
                       std::forward_as_tuple(S->ValueDG->getSingleResultInst()),
                       std::forward_as_tuple())
              .first->second.push_back(S);
        }
      }
    }
  }
}

void StaticStreamRegionAnalyzer::fuseLoadOps(StaticStream *S) {
  LLVM_DEBUG(llvm::dbgs() << "==== FuseLoadOps for " << S->getStreamName()
                          << '\n');
  /**
   * We maintain two sets:
   * 1. The set of instructions we have fused (visited).
   * 2. The current frontier instructions.
   * While processing a frontier instruction, we add its user to the next
   * frontier iff.
   * 1. All users from the same BB.
   * 2. The user only takes input from fused instructions or outside the
   * configure loop.
   * 3. The user is not one of these types:
   *    Load, Store, AtomicRMW, AtomicCmpXchg, Phi, Invoke,
   *    Call (except some intrinsics).
   * 4. The user has data type size <= My data type size.
   *
   * We start with all streams with a small AliasOffset to this stream, as
   * these streams are likely to be fused into a single stream. Also as a
   * heuristic, we only try to fuse for the stream with AliasOffset in the
   * middle. This is helpful to make the result aligned with consuming streams,
   * e.g., 2D convolution.
   *
   * During this process, we mark candidate final value if the current frontier
   * has only one instruction and the current fused instructions has no outside
   * users (except the one in the frontier).
   *
   * Terminate when the frontier is empty.
   */

  // Helper fundtion to check all users are instructions from the same BB.
  auto AreUserInstsFromSameBB = [S](const llvm::Instruction *Inst) -> bool {
    for (auto User : Inst->users()) {
      auto UserInst = llvm::dyn_cast<llvm::Instruction>(User);
      if (!UserInst) {
        // User is not even an instruction.
        return false;
      }
      if (UserInst->getParent() != S->Inst->getParent()) {
        return false;
      }
    }
    return true;
  };

  if (!AreUserInstsFromSameBB(S->Inst)) {
    LLVM_DEBUG(llvm::dbgs()
               << "[FuseLoadOps] No fusing as user not in the same BB.\n");
    return;
  }

  if (!S->AliasBaseStream) {
    LLVM_DEBUG(llvm::dbgs()
               << "[FuseLoadOps] No fusing as no AliasBaseS.\n");
    return;
  }

  StaticStream::InstVec FusedOps;
  StaticStream::InstSet FusedOpsSet;
  StaticStream::InstSet Frontier;
  StaticStream::InstSet NextFrontier;

  // Helper function to check all operands are used.
  auto AreUsedInstsFusedOrInvariant =
      [S, &FusedOpsSet](const llvm::Instruction *Inst) -> bool {
    for (auto OperandIdx = 0; OperandIdx < Inst->getNumOperands();
         ++OperandIdx) {
      auto Operand = Inst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
        if (!S->ConfigureLoop->contains(OperandInst)) {
          // From outside the ConfigureLoop.
          continue;
        }
        if (FusedOpsSet.count(OperandInst)) {
          // Already fused.
          continue;
        }
        return false;
      }
    }
    return true;
  };

  // Helper function to check if the instruction type can be fused.
  auto CanInstTypeBeFused = [](const llvm::Instruction *Inst) -> bool {
    if (Inst->isTerminator()) {
      return false;
    }
    switch (Inst->getOpcode()) {
    default:
      return true;
    case llvm::Instruction::Load:
    case llvm::Instruction::Store:
    case llvm::Instruction::PHI:
    case llvm::Instruction::Invoke:
    case llvm::Instruction::AtomicRMW:
    case llvm::Instruction::AtomicCmpXchg:
      return false;
    case llvm::Instruction::Call: {
      // With exception to some intrinsic.
      auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst);
      auto IntrinsicID = CallInst->getIntrinsicID();
      switch (IntrinsicID) {
      default:
        return false;
      case llvm::Intrinsic::x86_avx2_packusdw:
      case llvm::Intrinsic::x86_sse2_packuswb_128:
        return true;
      }
    }
    }
  };

  // Helper function to check the instruction's data type size <= mine.
  auto HasDataSizeLEMine = [S](const llvm::Instruction *Inst) -> bool {
    return S->DataLayout->getTypeStoreSize(Inst->getType()) <=
           S->getMemElementSize();
  };

  /**
   * Collect streams with small AliasOffset to myself.
   */
  StaticStream::StreamVec NearbyStreams;
  for (auto AliasS : S->AliasBaseStream->AliasedStreams) {
    const int64_t MaxAliasOffset = 16;
    auto AliasOffset = std::abs(AliasS->AliasOffset - S->AliasOffset);
    if (AliasOffset <= MaxAliasOffset &&
        AliasS->Inst->getOpcode() == S->Inst->getOpcode()) {
      NearbyStreams.push_back(AliasS);
    }
    if (Utils::isStoreInst(AliasS->Inst)) {
      LLVM_DEBUG(llvm::dbgs()
                 << "[FuseLoadOps] No fusing with AliasedStore.\n");
      return;
    }
  }
  std::sort(NearbyStreams.begin(), NearbyStreams.end(),
            [](const StaticStream *S1, const StaticStream *S2) -> bool {
              return S1->AliasOffset < S2->AliasOffset;
            });

  LLVM_DEBUG({
    for (auto S : NearbyStreams) {
      llvm::dbgs() << "[FuseLoadOps] Add NearBy Stream " << S->getStreamName()
                   << " AliasOffset " << S->AliasOffset << '\n';
    }
  });

  if (NearbyStreams.at(NearbyStreams.size() / 2) != S) {
    LLVM_DEBUG(llvm::dbgs()
               << "[FuseLoadOps] No fusing as I am not the MiddleOne.\n");
    return;
  }

  auto NearbyMemSize = S->getMemElementSize();
  for (auto S : NearbyStreams) {
    int AliasMemSize = S->getMemElementSize() + S->AliasOffset -
                       NearbyStreams.front()->AliasOffset;
    NearbyMemSize = std::max(NearbyMemSize, AliasMemSize);
  }

  // Also check that my size > 4 so that it is benefitial to fuse.
  if (NearbyMemSize <= 4 && S->Inst->getOpcode() == llvm::Instruction::Load) {
    LLVM_DEBUG(llvm::dbgs() << "[FuseLoadOps] No fusing as MyMemElementSize "
                            << S->getMemElementSize() << " too small.\n");
    return;
  }

  /**
   * If user specified LoadCompute group, we add additional LoadStreams as
   * input.
   */
  auto LoadComputeInputStreams = NearbyStreams;
  if (S->UserLoadComputeGroupIdx !=
      StaticStream::UserInvalidLoadComputeGroupIdx) {
    for (const auto &InstLoopStream : this->InstStaticStreamMap) {
      auto Inst = InstLoopStream.first;
      if (auto InstS =
              this->getStreamByInstAndConfigureLoop(Inst, S->ConfigureLoop)) {
        if (InstS->UserLoadComputeGroupIdx == S->UserLoadComputeGroupIdx) {
          bool Found = false;
          for (auto InputS : LoadComputeInputStreams) {
            if (InputS == InstS) {
              Found = true;
              break;
            }
          }
          if (!Found) {
            // This is a new LoadComputeInput stream.
            LLVM_DEBUG(llvm::dbgs() << "[FuseLoadOps] Add LoadComputeInputS "
                                    << InstS->getStreamName() << "\n");
            LoadComputeInputStreams.push_back(InstS);
          }
        }
      }
    }
  }

  for (auto S : LoadComputeInputStreams) {
    FusedOps.push_back(S->Inst);
    FusedOpsSet.insert(S->Inst);
    Frontier.insert(S->Inst);
  }

  while (!Frontier.empty()) {

    LLVM_DEBUG(llvm::dbgs()
                   << "[FuseLoadOps] ===== Start Processing Frontier.\n";);

    for (auto *CurInst : Frontier) {
      LLVM_DEBUG(llvm::dbgs() << "[FuseLoadOps] Process frontier inst "
                              << Utils::formatLLVMInst(CurInst) << '\n');
      for (auto User : CurInst->users()) {
        auto UserInst = llvm::dyn_cast<llvm::Instruction>(User);
        assert(UserInst && "User should always be an instruction.");
        if (!AreUserInstsFromSameBB(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "[FuseLoadOps] Not fuse !AreUserInstsFromSameBB "
                     << Utils::formatLLVMInst(UserInst) << '\n');
          continue;
        }
        if (!AreUsedInstsFusedOrInvariant(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "[FuseLoadOps] Not fuse !AreUsedInstsFusedOrInvariant "
                     << Utils::formatLLVMInst(UserInst) << '\n');
          continue;
        }
        if (!CanInstTypeBeFused(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "[FuseLoadOps] Not fuse !CanInstTypeBeFused "
                     << Utils::formatLLVMInst(UserInst) << '\n');
          continue;
        }
        if (!HasDataSizeLEMine(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "[FuseLoadOps] Not fuse !HasDataSizeLEMine "
                     << Utils::formatLLVMInst(UserInst) << '\n');
          continue;
        }
        // We passed all the test, try to add to next frontier.
        LLVM_DEBUG(llvm::dbgs() << "[FuseLoadOps] Fused "
                                << Utils::formatLLVMInst(UserInst) << '\n');
        NextFrontier.insert(UserInst);
      }
    }

    /**
     * Check if we found a candidate final inst.
     * NOTE: For now we require the final inst has smaller data type size to be
     * NOTE: profitable.
     */
    if (NextFrontier.size() == 1) {
      auto CandidateFinalInst = *NextFrontier.begin();

      LLVM_DEBUG({
        llvm::dbgs() << "[FuseLoadOps] ===== CandidateFinalInst "
                     << Utils::formatLLVMInst(CandidateFinalInst) << "\n";
      });

      bool FormClosure = true;
      for (auto FusedInst : FusedOps) {
        bool AllUserFused = true;
        for (auto User : FusedInst->users()) {
          auto UserInst = llvm::dyn_cast<llvm::Instruction>(User);
          assert(UserInst && "User should always be an instruction.");
          if (!FusedOpsSet.count(UserInst) && UserInst != CandidateFinalInst) {
            LLVM_DEBUG({
              llvm::dbgs() << "[FuseLoadOps] Outside User "
                           << Utils::formatLLVMInst(FusedInst) << " -> "
                           << Utils::formatLLVMInst(UserInst) << "\n";
            });
            AllUserFused = false;
            break;
          }
        }
        if (!AllUserFused) {
          FormClosure = false;
          break;
        }
      }
      if (FormClosure && this->DataLayout->getTypeStoreSize(
                             CandidateFinalInst->getType()) < NearbyMemSize) {
        // We found one closure.
        // Notice that we have to skip the first MyInst and add the
        // CandidateFinalInst
        S->FusedLoadOps.clear();
        S->FusedLoadOps.insert(S->FusedLoadOps.end(), FusedOps.begin() + 1,
                               FusedOps.end());
        S->FusedLoadOps.push_back(CandidateFinalInst);
        LLVM_DEBUG({
          llvm::dbgs() << "[FuseLoadOps] ===== Found Closure.\n";
          for (auto Inst : S->FusedLoadOps) {
            llvm::dbgs() << "[FuseLoadOps] == " << Utils::formatLLVMInst(Inst)
                         << '\n';
          }
        });
      }
    }

    FusedOps.insert(FusedOps.end(), NextFrontier.begin(), NextFrontier.end());
    FusedOpsSet.insert(NextFrontier.begin(), NextFrontier.end());

    Frontier = NextFrontier;
    NextFrontier.clear();
  }

  if (S->FusedLoadOps.empty()) {
    return;
  }

  // Remember the ValueDep.
  for (auto InputS : LoadComputeInputStreams) {
    if (InputS == S) {
      continue;
    }
    InputS->LoadStoreDepStreams.insert(S);
    S->LoadStoreBaseStreams.insert(InputS);
  }

  // Create the ValueDG if this is a LoadStream.
  // ValueDG for AtomicStream will be created by StaticStreamRegionAnalyzer.
  if (S->Inst->getOpcode() == llvm::Instruction::Load) {
    S->ValueDG = std::make_unique<StreamDataGraph>(
        S->ConfigureLoop, S->FusedLoadOps.back(),
        [](const llvm::Instruction *) -> bool { return false; });
  }
}

void StaticStreamRegionAnalyzer::buildStreamValueDepGraph() {
  /**
   * After construct addr dependence graph, we can analyze the
   * ValueDG and Value dependence.
   * We handle AtomicRMW stream as a special store stream.
   */
  if (!StreamPassEnableValueDG) {
    return;
  }
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    if (Utils::isStoreInst(Inst) ||
        Inst->getOpcode() == llvm::Instruction::AtomicRMW ||
        Inst->getOpcode() == llvm::Instruction::AtomicCmpXchg) {
      for (auto &S : InstStream.second) {
        this->buildValueDepForStoreOrAtomic(S);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::buildValueDepForStoreOrAtomic(
    StaticStream *StoreS) {

  // StoreS could be an Atomic stream.
  const auto StreamOpcode = StoreS->Inst->getOpcode();
  bool IsAtomic = (StreamOpcode == llvm::Instruction::AtomicRMW) ||
                  (StreamOpcode == llvm::Instruction::AtomicCmpXchg);

  LLVM_DEBUG(llvm::dbgs() << "Build ValueDG for " << StoreS->getStreamName()
                          << '\n');

  if (StoreS->BaseStepRootStreams.size() != 1) {
    // Wierd stream has no step root stream. We do not try to analyze it.
    LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] Multiple StepRoot.\n");
    return;
  }

  /**
   * We never try to build ValueDG on conditional store.
   */
  {
    auto StoreBB = StoreS->Inst->getParent();
    auto HeaderBB = StoreS->InnerMostLoop->getHeader();
    if (!this->PDT->dominates(StoreBB, HeaderBB)) {
      LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] Conditional Access.\n");
      return;
    }
  }

  /**
   * Let's try to construct the ValueDG:
   * 1. Every PHINode as an induction variable and later enforce all the
   * constraints.
   * 2. We also check directly check if its ReduceFinalInst of some
   * ReduceStream.
   */
  auto IsIndVarStream = [this](const llvm::Instruction *Inst) -> bool {
    if (llvm::isa<llvm::PHINode>(Inst)) {
      return true;
    }
    return this->FinalInstStaticStreamMap.count(Inst);
  };
  llvm::Value *StoreValue = nullptr;
  if (Utils::isStoreInst(StoreS->Inst)) {
    StoreValue = Utils::getStoreValue(StoreS->Inst);
  } else {
    switch (StreamOpcode) {
    default:
      llvm_unreachable("Invalid StoreStream Opcode.");
    case llvm::Instruction::AtomicRMW:
      StoreValue = StoreS->Inst->getOperand(1);
      break;
    case llvm::Instruction::AtomicCmpXchg:
      StoreValue = StoreS->Inst->getOperand(2);
      break;
    }
  }
  auto ValueDG = std::make_unique<StreamDataGraph>(StoreS->ConfigureLoop,
                                                   StoreValue, IsIndVarStream);
  /**
   * Enforce all the constraints.
   * 1. ValueDG should have no circle.
   * 2. ValueDG should only have Load/LoopInvariant inputs, at most 8
   * inputs. Notice that 3D stencil takes 8 inputs. This is to avoid
   * using stack to pass in parameters for the ValueDG.
   * 3. Should have static map to all load inputs. This enables the
   * stream to use outer loop streams value.
   */
  if (ValueDG->hasCircle()) {
    LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] Has Circle.\n");
    return;
  }
  if (ValueDG->hasPHINodeInComputeInsts() ||
      ValueDG->hasCallInstInComputeInsts()) {
    LLVM_DEBUG(llvm::dbgs()
               << "[NoValueDG] Has PHINode "
               << ValueDG->hasPHINodeInComputeInsts() << " CallInst "
               << ValueDG->hasCallInstInComputeInsts() << '\n');
    return;
  }
  if (ValueDG->getInputs().size() > 8) {
    LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] Too many Input "
                            << ValueDG->getInputs().size() << '\n');
    return;
  }
  std::unordered_set<const llvm::Instruction *> LoopVariantInputInsts;
  for (auto Input : ValueDG->getInputs()) {
    if (!this->isLegalValueDepInput(Input)) {
      // This is not a supported input type.
      LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] Illegal Input Type ";
                 Input->getType()->print(llvm::dbgs()); llvm::dbgs() << '\n');
      return;
    }
    if (auto InputInst = llvm::dyn_cast<llvm::Instruction>(Input)) {
      if (!StoreS->ConfigureLoop->contains(InputInst)) {
        // This is an input for the configuration.
        continue;
      }
      LoopVariantInputInsts.insert(InputInst);
    }
  }
  StaticStream::StreamVec InputStrams;
  for (auto InputInst : LoopVariantInputInsts) {
    auto InputS =
        this->getStreamWithPlaceholderInst(InputInst, StoreS->ConfigureLoop);
    if (!InputS) {
      // This is not a Stream.
      LLVM_DEBUG(llvm::dbgs()
                 << "[NoValueDG] LoopVariantInput Not Stream "
                 << Utils::formatLLVMInstWithoutFunc(InputInst) << '\n');
      return;
    }
    auto InnerLoop = StoreS->InnerMostLoop;
    auto OuterLoop = InputS->InnerMostLoop;
    if (StoreS->InnerMostLoop->contains(InputS->InnerMostLoop)) {
      // The opposite way, e.g., using final reduced value.
      InnerLoop = InputS->InnerMostLoop;
      OuterLoop = StoreS->InnerMostLoop;
    }
    if (!LoopUtils::hasLoopInvariantTripCountBetween(
            this->SE, StoreS->ConfigureLoop, InnerLoop, OuterLoop)) {
      LLVM_DEBUG(llvm::dbgs() << "[NoValueDG] No StaticMap: "
                              << InputS->getStreamName() << '\n');
      return;
    }
    InputStrams.push_back(InputS);
  }
  /**
   * We passed all checks, we remember this relationship.
   */
  for (auto InputS : InputStrams) {
    InputS->LoadStoreDepStreams.insert(StoreS);
    StoreS->LoadStoreBaseStreams.insert(InputS);
  }
  /**
   * Extend the ValueDG with the final atomic operation.
   */
  if (IsAtomic) {
    ValueDG->extendTailAtomicInst(StoreS->Inst, StoreS->FusedLoadOps);
  }
  StoreS->ValueDG = std::move(ValueDG);
}

void StaticStreamRegionAnalyzer::analyzeIsCandidate() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      S->analyzeIsCandidate();
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "analyzeIsCandidate done.\n");
}

bool StaticStreamRegionAnalyzer::isLegalValueDepInput(
    const llvm::Value *Value) const {
  auto Type = Value->getType();
  auto TypeSize = this->DataLayout->getTypeStoreSize(Type);
  if (TypeSize > 64) {
    return false;
  }
  if (Type->isIntOrPtrTy() || Type->isFloatTy() || Type->isDoubleTy()) {
    return true;
  }
  // We also allow vector type.
  if (auto VecType = llvm::dyn_cast<llvm::VectorType>(Type)) {
    auto ElementType = VecType->getElementType();
    if (ElementType->isIntOrPtrTy() || ElementType->isFloatTy() ||
        ElementType->isDoubleTy()) {
      return true;
    }
  }
  return false;
}

void StaticStreamRegionAnalyzer::markQualifiedStreams() {
  LLVM_DEBUG(llvm::dbgs() << "==== SSRA: MarkQualifiedStreams\n");
  std::list<StaticStream *> Queue;
  // First stage: ignore back-edge dependence.
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      auto IsQualified = S->checkIsQualifiedWithoutBackEdgeDep();
      if (IsQualified) {
        Queue.emplace_back(S);
      }
    }
  }

  /**
   * BFS on the qualified streams to propagate the qualified signal.
   */
  while (!Queue.empty()) {
    auto S = Queue.front();
    Queue.pop_front();
    if (S->isQualified()) {
      // We have already processed this stream.
      continue;
    }
    if (!S->checkIsQualifiedWithoutBackEdgeDep()) {
      assert(false && "Stream should be qualified to be inserted into the "
                      "qualifying queue.");
    }
    LLVM_DEBUG(llvm::dbgs() << "Mark Qualified " << S->getStreamName() << '\n');
    S->setIsQualified(true);
    // Check all the dependent streams.
    for (const auto &DependentStream : S->DependentStreams) {
      if (DependentStream->checkIsQualifiedWithoutBackEdgeDep()) {
        Queue.emplace_back(DependentStream);
      }
    }
  }
}

void StaticStreamRegionAnalyzer::enforceBackEdgeDependence() {
  /**
   * For IVStreams with back edge loads, we assume they are qualified from the
   * beginning point. Now it's time to check if their base memory streams are
   * actually qualified. If not, we need to propagate the dequalified signal
   * along the dependence chain.
   */
  std::list<StaticStream *> DisqualifiedQueue;
  for (auto &InstStream : this->InstStaticStreamMap) {
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        if (!S->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(S);
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
    S->setIsQualified(false);

    // Propagate to dependent streams.
    for (const auto &DependentStream : S->DependentStreams) {
      if (DependentStream->isQualified()) {
        if (!DependentStream->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(DependentStream);
        }
      }
    }

    // Also need to check back edges.
    for (const auto &DependentStream : S->BackIVDependentStreams) {
      if (DependentStream->isQualified()) {
        if (!DependentStream->checkIsQualifiedWithBackEdgeDep()) {
          DisqualifiedQueue.emplace_back(DependentStream);
        }
      }
    }
  }
}

void StaticStreamRegionAnalyzer::finalizePlan() {
  this->chooseStreams();
  LLVM_DEBUG(llvm::dbgs() << "Choose streams done.\n");
  this->buildChosenStreamDependenceGraph();
  LLVM_DEBUG(llvm::dbgs() << "Build chosen stream graph done.\n");
  this->buildTransformPlan();
  LLVM_DEBUG(llvm::dbgs() << "Build transform plan done.\n");
  this->buildStreamConfigureLoopInfoMap(this->TopLoop);
  LLVM_DEBUG(llvm::dbgs() << "Build StreamConfigureLoopInfoMap done.\n");

  this->dumpTransformPlan();
  this->dumpConfigurePlan();
}

void StaticStreamRegionAnalyzer::chooseStreams() {
  assert(StreamPassChooseStrategy !=
             StreamPassChooseStrategyE::DYNAMIC_OUTER_MOST &&
         "Invalid StreamChooseStrategy DynamicOuterMost for "
         "StaticStreamRegionAnalyzer");
  if (StreamPassChooseStrategy ==
      StreamPassChooseStrategyE::STATIC_OUTER_MOST) {
    this->chooseStreamAtStaticOuterMost();
  } else {
    this->chooseStreamAtInnerMost();
  }
}

void StaticStreamRegionAnalyzer::chooseStreamAtInnerMost() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    StaticStream *ChosenStream = nullptr;
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

void StaticStreamRegionAnalyzer::chooseStreamAtStaticOuterMost() {
  for (auto &InstStream : this->InstStaticStreamMap) {
    auto Inst = InstStream.first;
    StaticStream *ChosenStream = nullptr;
    int ChosenNumQualifiedDepStreams = -1;
    for (auto &S : InstStream.second) {
      if (S->isQualified()) {
        LLVM_DEBUG(llvm::dbgs() << "==== Choose stream StaticOuterMost for "
                                << S->getStreamName() << '\n');
        /**
         * Instead of just choose the out-most loop, we use
         * a heuristic to select the level with most number
         * of dependent streams qualified.
         * TODO: Fully check all dependent streams.
         */
        int NumQualifiedDepStreams = 0;
        std::vector<StaticStream *> Stack;
        std::set<StaticStream *> QualifiedDepStreams;
        for (auto DepSS : S->DependentStreams) {
          Stack.push_back(DepSS);
        }
        for (auto DepSS : S->BackIVDependentStreams) {
          Stack.push_back(DepSS);
        }
        while (!Stack.empty()) {
          auto DepSS = Stack.back();
          Stack.pop_back();
          if (QualifiedDepStreams.count(DepSS)) {
            continue;
          }
          if (DepSS->isQualified()) {
            NumQualifiedDepStreams++;
            QualifiedDepStreams.insert(DepSS);
            LLVM_DEBUG(llvm::dbgs() << "====== Qualified DepS "
                                    << DepSS->getStreamName() << '\n');
            for (auto DDS : DepSS->DependentStreams) {
              Stack.push_back(DDS);
            }
            for (auto BDS : DepSS->BackIVDependentStreams) {
              Stack.push_back(BDS);
            }
          }
        }
        LLVM_DEBUG(llvm::dbgs()
                   << "======= # Qualified DepS = " << NumQualifiedDepStreams
                   << ", ChosenNumQualifiedDepStreams = "
                   << ChosenNumQualifiedDepStreams << '\n');
        if (NumQualifiedDepStreams >= ChosenNumQualifiedDepStreams) {
          ChosenStream = S;
          ChosenNumQualifiedDepStreams = NumQualifiedDepStreams;
        }
      }
    }
    if (ChosenStream != nullptr) {
      this->InstChosenStreamMap.emplace(Inst, ChosenStream);
      ChosenStream->markChosen();
      LLVM_DEBUG(llvm::dbgs()
                 << "== Choose " << ChosenStream->getStreamName() << '\n');
    }
  }
}

void StaticStreamRegionAnalyzer::buildChosenStreamDependenceGraph() {
  for (auto &InstStream : this->InstChosenStreamMap) {
    auto &S = InstStream.second;
    S->constructChosenGraph();
  }
}

void StaticStreamRegionAnalyzer::buildTransformPlan() {
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

    LLVM_DEBUG(llvm::dbgs() << "make transform plan for stream "
                            << S->getStreamName() << '\n');

    // Handle all the step instructions.
    for (const auto &StepInst : S->getStepInsts()) {
      this->InstPlanMap.at(StepInst).planToStep(S);
      LLVM_DEBUG(llvm::dbgs() << "Select transform plan for inst "
                              << Utils::formatLLVMInst(StepInst) << " to "
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
        LLVM_DEBUG(llvm::dbgs()
                   << "Add used stream for user " << Utils::formatLLVMInst(I)
                   << " with stream " << S->getStreamName() << '\n');
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

    LLVM_DEBUG(llvm::dbgs()
               << "Select transform plan for inst "
               << Utils::formatLLVMInst(SelfInst) << " to "
               << StreamTransformPlan::formatPlanT(SelfPlan.Plan) << '\n');
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

void StaticStreamRegionAnalyzer::buildStreamConfigureLoopInfoMap(
    const llvm::Loop *ConfigureLoop) {
  assert(this->ConfigureLoopInfoMap.count(ConfigureLoop) == 0 &&
         "This StreamConfigureLoopInfo has already been built.");
  auto SortedStreams = this->sortChosenStreamsByConfigureLoop(ConfigureLoop);
  this->ConfigureLoopInfoMap.emplace(
      std::piecewise_construct, std::forward_as_tuple(ConfigureLoop),
      std::forward_as_tuple(this->AnalyzePath, this->AnalyzeRelativePath,
                            ConfigureLoop, SortedStreams));
  this->allocateRegionStreamId(ConfigureLoop);
  for (auto &SubLoop : *ConfigureLoop) {
    this->buildStreamConfigureLoopInfoMap(SubLoop);
  }
}

void StaticStreamRegionAnalyzer::allocateRegionStreamId(
    const llvm::Loop *ConfigureLoop) {
  auto &ConfigureLoopInfo = this->getConfigureLoopInfo(ConfigureLoop);
  int UsedRegionId =
      ::LLVM::TDG::ReservedStreamRegionId::NumReservedStreamRegionId;
  for (auto ParentLoop = ConfigureLoop->getParentLoop();
       ParentLoop != nullptr && this->TopLoop->contains(ParentLoop);
       ParentLoop = ParentLoop->getParentLoop()) {
    const auto &ParentConfigureLoopInfo =
        this->getConfigureLoopInfo(ParentLoop);
    UsedRegionId += ParentConfigureLoopInfo.getSortedStreams().size();
  }
  for (auto S : ConfigureLoopInfo.getSortedStreams()) {
    const int MaxRegionStreamId = 128;
    assert(UsedRegionId < MaxRegionStreamId && "RegionStreamId overflow.");
    S->setRegionStreamId(UsedRegionId++);
  }
}

const StreamConfigureLoopInfo &StaticStreamRegionAnalyzer::getConfigureLoopInfo(
    const llvm::Loop *ConfigureLoop) const {
  assert(this->TopLoop->contains(ConfigureLoop) &&
         "ConfigureLoop should be within TopLoop.");
  assert(this->ConfigureLoopInfoMap.count(ConfigureLoop) != 0 &&
         "Failed to find the loop info.");
  return this->ConfigureLoopInfoMap.at(ConfigureLoop);
}

StreamConfigureLoopInfo &StaticStreamRegionAnalyzer::getConfigureLoopInfo(
    const llvm::Loop *ConfigureLoop) {
  assert(this->TopLoop->contains(ConfigureLoop) &&
         "ConfigureLoop should be within TopLoop.");
  assert(this->ConfigureLoopInfoMap.count(ConfigureLoop) != 0 &&
         "Failed to find the loop info.");
  return this->ConfigureLoopInfoMap.at(ConfigureLoop);
}

std::vector<StaticStream *>
StaticStreamRegionAnalyzer::sortChosenStreamsByConfigureLoop(
    const llvm::Loop *ConfigureLoop) {
  /**
   * Topological sort is not enough for this problem, as some streams may be
   * coalesced. Instead, we define the dependenceDepth of a stream as:
   * 1. A stream with no base stream in the same ConfigureLoop has
   * dependenceDepth 0;
   * 2. Otherwiese, a stream has dependenceDepth =
   * max(BaseStreamWithSameConfigureLoop.dependenceDepth) + 1.
   *
   * With the same DepDepth, we break the tie with stream id, as we allocate
   * them in order, we should be able to ensure new value dependence.
   */
  std::stack<std::pair<StaticStream *, int>> ChosenStreams;
  std::vector<StaticStream *> SortedStreams;
  for (auto &InstChosenStream : this->InstChosenStreamMap) {
    auto &S = InstChosenStream.second;
    if (S->ConfigureLoop == ConfigureLoop) {
      ChosenStreams.emplace(S, 0);
      SortedStreams.push_back(S);
    }
  }

  std::unordered_map<StaticStream *, int> StreamDepDepthMap;
  while (!ChosenStreams.empty()) {
    auto &Entry = ChosenStreams.top();
    auto &S = Entry.first;
    if (Entry.second == 0) {
      Entry.second = 1;
      for (auto &ChosenBaseS : S->ChosenBaseStreams) {
        if (ChosenBaseS->ConfigureLoop != ConfigureLoop) {
          continue;
        }
        if (StreamDepDepthMap.count(ChosenBaseS) == 0) {
          ChosenStreams.emplace(ChosenBaseS, 0);
        }
      }
    } else {
      if (StreamDepDepthMap.count(S) == 0) {
        // Determine my dependenceDepth.
        auto DependenceDepth = 0;
        for (auto &ChosenBaseS : S->ChosenBaseStreams) {
          if (ChosenBaseS->ConfigureLoop != ConfigureLoop) {
            continue;
          }
          assert(
              (StreamDepDepthMap.count(ChosenBaseS) != 0) &&
              "ChosenBaseStream should already be assigned a dependenceDepth.");
          auto ChosenBaseStreamDependenceDepth =
              StreamDepDepthMap.at(ChosenBaseS);
          if (ChosenBaseStreamDependenceDepth + 1 > DependenceDepth) {
            DependenceDepth = ChosenBaseStreamDependenceDepth + 1;
          }
        }
        StreamDepDepthMap.emplace(S, DependenceDepth);
      }
      ChosenStreams.pop();
    }
  }

  // Finally we sort the streams.
  std::sort(SortedStreams.begin(), SortedStreams.end(),
            [&StreamDepDepthMap](StaticStream *A, StaticStream *B) -> bool {
              auto DepthA = StreamDepDepthMap.at(A);
              auto DepthB = StreamDepDepthMap.at(B);
              if (DepthA != DepthB) {
                return DepthA < DepthB;
              } else {
                return A->StreamId < B->StreamId;
              }
            });

  return SortedStreams;
}

void StaticStreamRegionAnalyzer::coalesceStreamsAtLoop(llvm::Loop *Loop) {
  const auto &ConfigureInfo = this->getConfigureLoopInfo(Loop);
  if (ConfigureInfo.TotalConfiguredStreams == 0) {
    // No streams here.
    return;
  }
  /**
   * We coalesce memory streams if:
   * 1. They share the same step root.
   * 2. They have scev.
   * 3. Their SCEV has a constant offset.
   */
  std::list<std::vector<std::pair<StaticStream *, int64_t>>> CoalescedGroup;
  for (auto SS : ConfigureInfo.getSortedStreams()) {
    LLVM_DEBUG(llvm::dbgs() << "====== Try to coalesce stream: "
                            << SS->getStreamName() << '\n');
    if (SS->BaseStepRootStreams.size() != 1 ||
        SS->Type == StaticStream::TypeT::IV) {
      // These streams are not coalesced, set the coalesce group to itself.
      SS->setCoalesceGroup(SS->StreamId, 0);
      continue;
    }
    auto Addr = const_cast<llvm::Value *>(Utils::getMemAddrValue(SS->Inst));
    auto AddrSCEV = SS->SE->getSCEV(Addr);

    bool Coalesced = false;
    if (AddrSCEV) {
      LLVM_DEBUG(llvm::dbgs() << "AddrSCEV: "; AddrSCEV->dump());
      for (auto &Group : CoalescedGroup) {
        auto TargetSS = Group.front().first;
        if (TargetSS->BaseStepRootStreams != SS->BaseStepRootStreams) {
          // Do not share the same step root.
          continue;
        }
        // Check the load/store.
        if (TargetSS->Inst->getOpcode() != SS->Inst->getOpcode()) {
          continue;
        }
        auto TargetAddr =
            const_cast<llvm::Value *>(Utils::getMemAddrValue(TargetSS->Inst));
        auto TargetAddrSCEV = TargetSS->SE->getSCEV(TargetAddr);
        if (!TargetAddrSCEV) {
          continue;
        }
        // Check the scev.
        LLVM_DEBUG(llvm::dbgs() << "TargetAddrSCEV: "; TargetAddrSCEV->dump());
        auto MinusSCEV = SS->SE->getMinusSCEV(AddrSCEV, TargetAddrSCEV);
        LLVM_DEBUG(llvm::dbgs() << "MinusSCEV: "; MinusSCEV->dump());
        auto OffsetSCEV = llvm::dyn_cast<llvm::SCEVConstant>(MinusSCEV);
        if (!OffsetSCEV) {
          // Not constant offset.
          continue;
        }
        LLVM_DEBUG(llvm::dbgs() << "OffsetSCEV: "; OffsetSCEV->dump());
        int64_t Offset = OffsetSCEV->getAPInt().getSExtValue();
        LLVM_DEBUG(llvm::dbgs()
                   << "Coalesced, offset: " << Offset
                   << " with stream: " << TargetSS->getStreamName() << '\n');
        Coalesced = true;
        Group.emplace_back(SS, Offset);
        break;
      }
    }

    if (!Coalesced) {
      LLVM_DEBUG(llvm::dbgs() << "New coalesce group\n");
      CoalescedGroup.emplace_back();
      CoalescedGroup.back().emplace_back(SS, 0);
    }
  }

  // Sort each group with increasing order of offset.
  for (auto &Group : CoalescedGroup) {
    std::sort(Group.begin(), Group.end(),
              [](const std::pair<StaticStream *, int64_t> &a,
                 const std::pair<StaticStream *, int64_t> &b) -> bool {
                return a.second < b.second;
              });
    // Set the base/offset.
    auto BaseS = Group.front().first;
    int64_t BaseOffset = Group.front().second;
    for (auto &StreamOffset : Group) {
      auto S = StreamOffset.first;
      auto Offset = StreamOffset.second;
      S->setCoalesceGroup(BaseS->StreamId, Offset - BaseOffset);
    }
  }
}

void StaticStreamRegionAnalyzer::endTransform() {
  // Finalize the StreamConfigureLoopInfo.
  this->finalizeStreamConfigureLoopInfo(this->TopLoop);
  // Only after this point can we know the CoalesceGroup in the info.
  this->dumpStreamInfos();
  // Dump all the StreamConfigureLoopInfo.
  for (const auto &StreamConfigureLoop : this->ConfigureLoopInfoMap) {
    StreamConfigureLoop.second.dump(this->DataLayout);
  }
}

/**
 * This function recursively compute the information for this configure loop.
 * 1. Compute TotalConfiguredStreams.
 * 2. Recursively compute TotalSubLoopStreams.
 * 3. Compute TotalAliveStreams.
 */
void StaticStreamRegionAnalyzer::finalizeStreamConfigureLoopInfo(
    const llvm::Loop *ConfigureLoop) {
  assert(this->TopLoop->contains(ConfigureLoop) &&
         "Should only finalize loops within TopLoop.");
  auto &Info = this->ConfigureLoopInfoMap.at(ConfigureLoop);
  // 1.
  auto &SortedStreams = Info.getSortedStreams();

  // Compute the coalesced streams.
  std::unordered_set<int> CoalescedGroupSet;
  for (auto &S : SortedStreams) {
    auto CoalesceGroup = S->getCoalesceGroup();
    if (CoalescedGroupSet.count(CoalesceGroup) == 0) {
      // First time we see this.
      Info.SortedCoalescedStreams.push_back(S);
      CoalescedGroupSet.insert(CoalesceGroup);
    }
  }

  int ConfiguredStreams = SortedStreams.size();
  int ConfiguredCoalescedStreams = Info.SortedCoalescedStreams.size();
  Info.TotalConfiguredStreams = ConfiguredStreams;
  Info.TotalConfiguredCoalescedStreams = ConfiguredCoalescedStreams;
  auto ParentLoop = ConfigureLoop->getParentLoop();
  if (this->TopLoop->contains(ParentLoop)) {
    const auto &ParentInfo = this->ConfigureLoopInfoMap.at(ParentLoop);
    Info.TotalConfiguredStreams += ParentInfo.TotalConfiguredStreams;
    Info.TotalConfiguredCoalescedStreams +=
        ParentInfo.TotalConfiguredCoalescedStreams;
  }
  // 2.
  int MaxSubLoopStreams = 0;
  int MaxSubLoopCoalescedStreams = 0;
  for (auto &SubLoop : *ConfigureLoop) {
    this->finalizeStreamConfigureLoopInfo(SubLoop);
    const auto &SubInfo = this->ConfigureLoopInfoMap.at(SubLoop);
    if (SubInfo.TotalSubLoopStreams > MaxSubLoopStreams) {
      MaxSubLoopStreams = SubInfo.TotalSubLoopStreams;
    }
    if (SubInfo.TotalSubLoopCoalescedStreams > MaxSubLoopCoalescedStreams) {
      MaxSubLoopCoalescedStreams = SubInfo.TotalSubLoopCoalescedStreams;
    }
  }
  Info.TotalSubLoopStreams = MaxSubLoopStreams + ConfiguredStreams;
  Info.TotalSubLoopCoalescedStreams =
      MaxSubLoopCoalescedStreams + ConfiguredCoalescedStreams;

  // 3.
  Info.TotalAliveStreams = Info.TotalConfiguredStreams +
                           Info.TotalSubLoopStreams - ConfiguredStreams;
  Info.TotalAliveCoalescedStreams = Info.TotalConfiguredCoalescedStreams +
                                    Info.TotalSubLoopCoalescedStreams -
                                    ConfiguredCoalescedStreams;
}

void StaticStreamRegionAnalyzer::dumpStreamInfos() {
  {
    LLVM::TDG::StreamRegion ProtobufStreamRegion;
    for (auto &InstStream : this->InstStaticStreamMap) {
      for (auto &S : InstStream.second) {
        auto Info = ProtobufStreamRegion.add_streams();
        S->fillProtobufStreamInfo(Info);
      }
    }

    auto InfoTextPath = this->getAnalyzePath() + "/streams.json";
    Utils::dumpProtobufMessageToJson(ProtobufStreamRegion, InfoTextPath);

    std::ofstream InfoFStream(this->getAnalyzePath() + "/streams.info");
    assert(InfoFStream.is_open() &&
           "Failed to open the output info protobuf file.");
    ProtobufStreamRegion.SerializeToOstream(&InfoFStream);
    InfoFStream.close();
  }

  {
    LLVM::TDG::StreamRegion ProtobufStreamRegion;
    for (auto &InstStream : this->InstChosenStreamMap) {
      auto &S = InstStream.second;
      auto Info = ProtobufStreamRegion.add_streams();
      S->fillProtobufStreamInfo(Info);
    }

    auto InfoTextPath = this->getAnalyzePath() + "/chosen_streams.json";
    Utils::dumpProtobufMessageToJson(ProtobufStreamRegion, InfoTextPath);
  }
}

void StaticStreamRegionAnalyzer::dumpTransformPlan() {
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
      ss << std::setw(50) << std::left << Utils::formatLLVMInst(Inst) << PlanStr
         << '\n';
    }
  }

  // Also dump to file.
  std::string PlanPath = this->getAnalyzePath() + "/plan.txt";
  std::ofstream PlanFStream(PlanPath);
  assert(PlanFStream.is_open() &&
         "Failed to open dump loop transform plan file.");
  PlanFStream << ss.str() << '\n';
  PlanFStream.close();
}

void StaticStreamRegionAnalyzer::dumpConfigurePlan() {
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
    const auto &Info = this->getConfigureLoopInfo(Loop);
    for (auto S : Info.getSortedStreams()) {
      for (auto Level = this->TopLoop->getLoopDepth();
           Level < Loop->getLoopDepth(); ++Level) {
        ss << "  ";
      }
      ss << S->getStreamName() << '\n';
    }
  }
  std::string PlanPath = this->getAnalyzePath() + "/config.plan.txt";
  std::ofstream PlanFStream(PlanPath);
  assert(PlanFStream.is_open() &&
         "Failed to open dump loop configure plan file.");
  PlanFStream << ss.str() << '\n';
  PlanFStream.close();
}

void StaticStreamRegionAnalyzer::nestRegionInto(
    const llvm::Loop *InnerLoop, const llvm::Loop *OuterLoop,
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> ConfigFuncInfo,
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> PredFuncInfo,
    bool PredicateRet) {
  auto &InnerConfigInfo = this->getConfigureLoopInfo(InnerLoop);
  auto &OuterConfigInfo = this->getConfigureLoopInfo(OuterLoop);
  OuterConfigInfo.addNestConfigureInfo(&InnerConfigInfo,
                                       std::move(ConfigFuncInfo),
                                       std::move(PredFuncInfo), PredicateRet);
}
