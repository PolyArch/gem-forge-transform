#include "stream/StaticStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

#define DEBUG_TYPE "StaticStream"

uint64_t StaticStream::AllocatedStreamId = 0;

/**
 * The constructor just creates the object and does not perform any analysis.
 *
 * After creating all the streams, the manager should call constructGraph() to
 * initialize the MetaGraph and StreamGraph.
 */
StaticStream::StaticStream(TypeT _Type, const llvm::Instruction *_Inst,
                           const llvm::Loop *_ConfigureLoop,
                           const llvm::Loop *_InnerMostLoop,
                           llvm::ScalarEvolution *_SE,
                           const llvm::PostDominatorTree *_PDT,
                           llvm::DataLayout *_DataLayout)
    : StreamId(allocateStreamId()), Type(_Type), Inst(_Inst),
      ConfigureLoop(_ConfigureLoop), InnerMostLoop(_InnerMostLoop),
      FuncNameBase(llvm::Twine(_Inst->getFunction()->getName() + "_" +
                               _Inst->getParent()->getName() + "_" +
                               _Inst->getName() + "_" + _Inst->getOpcodeName() +
                               "_" +
                               llvm::Twine(Utils::getLLVMInstPosInBB(_Inst)))
                       .str()),
      SE(_SE), PDT(_PDT), DataLayout(_DataLayout), IsCandidate(false),
      IsQualified(false), IsChosen(false), CoalesceGroup(StreamId),
      CoalesceOffset(0) {
  this->StaticStreamInfo.set_loop_level(this->InnerMostLoop->getLoopDepth());
  this->StaticStreamInfo.set_config_loop_level(
      this->ConfigureLoop->getLoopDepth());
  this->StaticStreamInfo.set_is_inner_most_loop(
      this->InnerMostLoop->getSubLoops().empty());

  this->fuseLoadOps();
}

::LLVM::TDG::DataType
StaticStream::translateToProtobufDataType(llvm::DataLayout *DataLayout,
                                          llvm::Type *Type) {
  if (auto IntType = llvm::dyn_cast<llvm::IntegerType>(Type)) {
    assert(IntType->getBitWidth() <= 64 && "IntType overflow.");
    return ::LLVM::TDG::DataType::INTEGER;
  } else if (Type->isPointerTy()) {
    return ::LLVM::TDG::DataType::INTEGER;
  } else if (Type->isFloatTy()) {
    return ::LLVM::TDG::DataType::FLOAT;
  } else if (Type->isDoubleTy()) {
    return ::LLVM::TDG::DataType::DOUBLE;
  } else if (Type->isVectorTy()) {
    auto NumBits = DataLayout->getTypeStoreSizeInBits(Type);
    switch (NumBits) {
    case 128:
      return ::LLVM::TDG::DataType::VECTOR_128;
    case 256:
      return ::LLVM::TDG::DataType::VECTOR_256;
    case 512:
      return ::LLVM::TDG::DataType::VECTOR_512;
    default:
      llvm::errs() << "Invalid Vector BitWidth " << NumBits << '\n';
      llvm_unreachable("Invalid Vector BitWidth.\n");
    }
  }
  llvm_unreachable("Invalid DataType.\n");
}

void StaticStream::setStaticStreamInfo(LLVM::TDG::StaticStreamInfo &SSI) const {
  SSI.CopyFrom(this->StaticStreamInfo);
  SSI.set_is_candidate(this->IsCandidate);
  SSI.set_is_qualified(this->IsQualified);
  // Set
#define ADD_STREAM(SET, FIELD, PRED_TRUE)                                      \
  {                                                                            \
    for (const auto &S : SET) {                                                \
      auto Entry = SSI.add_##FIELD();                                          \
      S->formatProtoStreamId(Entry->mutable_id());                             \
      Entry->set_pred_true(PRED_TRUE);                                         \
    }                                                                          \
  }
  ADD_STREAM(this->PredicatedTrueStreams, predicated_streams, true);
  ADD_STREAM(this->PredicatedFalseStreams, predicated_streams, false);
#undef ADD_STREAM

  // Set the alias stream information.
  if (this->AliasBaseStream) {
    this->AliasBaseStream->formatProtoStreamId(SSI.mutable_alias_base_stream());
    SSI.set_alias_offset(this->AliasOffset);
  }
  for (auto &S : this->AliasedStreams) {
    auto ProtoAliasedStreamId = SSI.add_aliased_streams();
    S->formatProtoStreamId(ProtoAliasedStreamId);
  }

  // Set the update relationship.
  auto ProtoStreamComputeInfo = SSI.mutable_compute_info();
  if (this->UpdateStream) {
    auto ProtoUpdateStreamId = ProtoStreamComputeInfo->mutable_update_stream();
    this->UpdateStream->formatProtoStreamId(ProtoUpdateStreamId);
  }

  // Set the element size.
  SSI.set_mem_element_size(this->getMemElementSize());
  SSI.set_core_element_size(this->getCoreElementSize());

  // Set data type if we can support it.
  SSI.set_core_element_type(
      this->translateToProtobufDataType(this->getCoreElementType()));
  SSI.set_mem_element_type(
      this->translateToProtobufDataType(this->getMemElementType()));
}

llvm::Type *StaticStream::getMemElementType() const {
  switch (this->Inst->getOpcode()) {
  case llvm::Instruction::PHI:
  case llvm::Instruction::Load:
  case llvm::Instruction::Call:
  case llvm::Instruction::AtomicRMW:
    return this->Inst->getType();
  case llvm::Instruction::AtomicCmpXchg:
  case llvm::Instruction::Store: {
    auto AddrType = Utils::getMemAddrValue(this->Inst)->getType();
    auto ElementType = AddrType->getPointerElementType();
    return ElementType;
  }
  default:
    llvm_unreachable("Unsupported StreamType.");
  }
}

int StaticStream::getMemElementSize() const {
  return this->DataLayout->getTypeStoreSize(this->getMemElementType());
}

llvm::Type *StaticStream::getCoreElementType() const {
  /**
   * In most case, the core element size is the same as the memory,
   * except for FusedLoadOps. This is only used for AtomicCmpXchg.
   */
  if (!this->FusedLoadOps.empty()) {
    return this->FusedLoadOps.back()->getType();
  } else {
    return this->getMemElementType();
  }
}

int StaticStream::getCoreElementSize() const {
  return this->DataLayout->getTypeStoreSize(this->getCoreElementType());
}

void StaticStream::handleFirstTimeComputeNode(
    std::list<DFSNode> &DFSStack, DFSNode &DNode,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap,
    GetStreamFuncT GetStream) {
  auto ComputeMNode = DNode.ComputeMNode;

  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(DNode.Value)) {
    if (this->ConfigureLoop->contains(Inst)) {
      if (llvm::isa<llvm::LoadInst>(Inst)) {
        // LoadBaseStream.
        auto LoadBaseStream = GetStream(Inst, this->ConfigureLoop);
        if (!LoadBaseStream) {
          llvm::errs() << "Failed to find LoadBaseStream "
                       << Utils::formatLLVMInst(Inst) << " for "
                       << this->formatName() << '\n';
          assert(false && "Failed to find LoadBaseStream.");
        }
        ComputeMNode->LoadBaseStreams.insert(LoadBaseStream);
        this->LoadBaseStreams.insert(LoadBaseStream);
        DFSStack.pop_back();
      } else if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        // We have to pop this before handleFirstTimePHIMetaNode emplace next
        // compute nodes.
        DFSStack.pop_back();
        if (auto IndVarBaseStream = GetStream(Inst, this->ConfigureLoop)) {
          // IndVarBaseStream.
          ComputeMNode->IndVarBaseStreams.insert(IndVarBaseStream);
          this->IndVarBaseStreams.insert(IndVarBaseStream);
        } else {
          // Another PHIMetaNode.
          if (ConstructedPHIMetaNodeMap.count(PHINode) == 0) {
            // First time encounter the phi node.
            this->PHIMetaNodes.emplace_back(PHINode);
            ConstructedPHIMetaNodeMap.emplace(PHINode,
                                              &this->PHIMetaNodes.back());
            // This may modify the DFSStack.
            this->handleFirstTimePHIMetaNode(DFSStack,
                                             &this->PHIMetaNodes.back(),
                                             ConstructedComputeMetaNodeMap);
          }
          auto PHIMNode = ConstructedPHIMetaNodeMap.at(PHINode);
          ComputeMNode->PHIMetaNodes.insert(PHIMNode);
        }
      } else if (Utils::isCallOrInvokeInst(Inst)) {
        // Call input.
        ComputeMNode->CallInputs.insert(Inst);
        this->CallInputs.insert(Inst);
        DFSStack.pop_back();
      } else {
        // Normal compute inst. Ignore if I am already processed.
        bool Inserted = false;
        for (const auto &ComputeInst : ComputeMNode->ComputeInsts) {
          if (ComputeInst == Inst) {
            Inserted = true;
            break;
          }
        }
        if (!Inserted) {
          for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            DFSStack.emplace_back(ComputeMNode, Inst->getOperand(OperandIdx));
            LLVM_DEBUG(llvm::dbgs()
                       << "Pushing DFSNode "
                       << Utils::formatLLVMValue(ComputeMNode->RootValue)
                       << Utils::formatLLVMValue(Inst->getOperand(OperandIdx))
                       << '\n');
          }
        }
      }
    } else {
      // Loop Invarient Inst.
      ComputeMNode->LoopInvariantInputs.insert(Inst);
      this->LoopInvariantInputs.insert(Inst);
      DFSStack.pop_back();
    }
  } else {
    // Loop Invarient Value.
    ComputeMNode->LoopInvariantInputs.insert(DNode.Value);
    this->LoopInvariantInputs.insert(DNode.Value);
    DFSStack.pop_back();
  }
}

void StaticStream::handleFirstTimePHIMetaNode(
    std::list<DFSNode> &DFSStack, PHIMetaNode *PHIMNode,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {
  auto PHINode = PHIMNode->PHINode;
  for (unsigned IncomingIdx = 0,
                NumIncomingValues = PHINode->getNumIncomingValues();
       IncomingIdx != NumIncomingValues; ++IncomingIdx) {
    auto BB = PHINode->getIncomingBlock(IncomingIdx);
    auto IncomingValue = PHINode->getIncomingValue(IncomingIdx);
    // Check if there is a constructed compute node.
    if (ConstructedComputeMetaNodeMap.count(IncomingValue) == 0) {
      // First time.
      const llvm::SCEV *SCEV = nullptr;
      if (this->SE->isSCEVable(IncomingValue->getType())) {
        SCEV = this->SE->getSCEV(IncomingValue);
      }
      this->ComputeMetaNodes.emplace_back(IncomingValue, SCEV);
      ConstructedComputeMetaNodeMap.emplace(IncomingValue,
                                            &this->ComputeMetaNodes.back());
      // Push to the stack.
      DFSStack.emplace_back(&this->ComputeMetaNodes.back(), IncomingValue);
      LLVM_DEBUG(llvm::dbgs() << "Pushing DFSNode "
                              << Utils::formatLLVMValue(
                                     this->ComputeMetaNodes.back().RootValue)
                              << Utils::formatLLVMValue(IncomingValue) << '\n');
    }
    // Add to my children.
    PHIMNode->ComputeMetaNodes.insert(
        ConstructedComputeMetaNodeMap.at(IncomingValue));
  }
}

void StaticStream::constructMetaGraph(GetStreamFuncT GetStream) {
  // Create the first PHIMetaNode.
  std::list<DFSNode> DFSStack;
  this->initializeMetaGraphConstruction(DFSStack,
                                        this->ConstructedPHIMetaNodeMap,
                                        this->ConstructedComputeMetaNodeMap);
  while (!DFSStack.empty()) {
    auto &DNode = DFSStack.back();
    LLVM_DEBUG(llvm::dbgs()
               << "Processing DFSNode "
               << Utils::formatLLVMValue(DNode.ComputeMNode->RootValue)
               << Utils::formatLLVMValue(DNode.Value) << " Visit time "
               << DNode.VisitTimes << '\n');
    if (DNode.VisitTimes == 0) {
      // First time.
      DNode.VisitTimes++;
      // Notice that handleFirstTimeComputeNode may pop DFSStack!.
      this->handleFirstTimeComputeNode(
          DFSStack, DNode, ConstructedPHIMetaNodeMap,
          ConstructedComputeMetaNodeMap, GetStream);
    } else {
      // Second time. This must be a real compute inst.
      bool Inserted = false;
      auto Inst = llvm::dyn_cast<llvm::Instruction>(DNode.Value);
      assert(Inst && "Only compute inst will be handled twice.");
      assert(!llvm::isa<llvm::PHINode>(Inst) &&
             "No PHINode can be handled twice.");
      for (auto ComputeInst : DNode.ComputeMNode->ComputeInsts) {
        if (ComputeInst == Inst) {
          Inserted = true;
          break;
        }
      }
      if (!Inserted) {
        // Add my self to the compute insts.
        // This will ensure the topological order.
        DNode.ComputeMNode->ComputeInsts.push_back(Inst);
      }
      DFSStack.pop_back();
    }
  }
}

void StaticStream::addBaseStream(StaticStream *Other) {
  LLVM_DEBUG(llvm::dbgs() << "== SS: AddBaseStream: "
                          << (Other ? Other->formatName() : "nullptr") << '\n');
  this->BaseStreams.insert(Other);
  if (Other != nullptr) {
    Other->DependentStreams.insert(this);
    if (Other->InnerMostLoop == this->InnerMostLoop) {
      this->BaseStepStreams.insert(Other);
    }
  }
}

void StaticStream::addBackEdgeBaseStream(StaticStream *Other) {
  this->BackMemBaseStreams.insert(Other);
  Other->BackIVDependentStreams.insert(this);
}

void StaticStream::constructStreamGraph() {
  // Basically translate from what we found in LoadBaseStreams and
  // IndVarBaseStreams during the construction of the MetaGraph to BaseStreams
  // and BackMemBaseStreams.
  for (auto LoadBaseStream : this->LoadBaseStreams) {
    if (LoadBaseStream == this) {
      // No circle dependency in stream graph.
      continue;
    }
    if (LoadBaseStream->InnerMostLoop == this->InnerMostLoop &&
        this->Type == TypeT::IV) {
      // This is a back edge dependence.
      this->addBackEdgeBaseStream(LoadBaseStream);
    } else {
      this->addBaseStream(LoadBaseStream);
    }
  }
  for (auto IndVarBaseStream : this->IndVarBaseStreams) {
    if (IndVarBaseStream == this) {
      // No circle dependency in stream graph.
      continue;
    }
    this->addBaseStream(IndVarBaseStream);
  }
}

void StaticStream::constructGraph(GetStreamFuncT GetStream) {
  // Construct the two graphs.
  this->constructMetaGraph(GetStream);
  LLVM_DEBUG(llvm::dbgs() << "== SS: ConstructMetaGraph done.\n");
  this->constructStreamGraph();
  LLVM_DEBUG(llvm::dbgs() << "== SS: ConstructStreamGraph done.\n");
  // Some misc analysis.
  this->analyzeIsConditionalAccess();
  this->analyzeIsTripCountFixed();
}

/**
 * This must happen after all the calls to addBaseStream.
 */
void StaticStream::computeBaseStepRootStreams() {
  for (auto &BaseStepStream : this->BaseStepStreams) {
    if (BaseStepStream->Type == StaticStream::TypeT::IV) {
      // Induction variable is always a root stream.
      this->BaseStepRootStreams.insert(BaseStepStream);
      // No need to go deeper for IVStream.
      continue;
    }
    if (BaseStepStream->BaseStepRootStreams.empty()) {
      // If this is empty, recompute (even if recomputed).
      BaseStepStream->computeBaseStepRootStreams();
    }
    for (auto &BaseStepRootStream : BaseStepStream->BaseStepRootStreams) {
      LLVM_DEBUG(llvm::dbgs() << "== SS: AddBaseStepRoot "
                              << BaseStepRootStream->formatName() << '\n');
      this->BaseStepRootStreams.insert(BaseStepRootStream);
    }
  }
}

bool StaticStream::checkBaseStreamInnerMostLoopContainsMine() const {
  for (const auto &BaseStream : this->BaseStreams) {
    if (!BaseStream->InnerMostLoop->contains(this->InnerMostLoop)) {
      return false;
    }
  }
  for (const auto &BackMemBaseStream : this->BackMemBaseStreams) {
    if (!BackMemBaseStream->InnerMostLoop->contains(this->InnerMostLoop)) {
      return false;
    }
  }
  return true;
}

bool StaticStream::checkStaticMapFromBaseStreamInParentLoop() const {
  // Noo need to worry about back edge base streams, cause they are guaranteed
  // to be not in a parent loop.
  assert(this->isCandidate() &&
         "Should not check static mapping for non-candidate stream.");
  auto MyStpPattern = this->computeStepPattern();
  for (const auto &BaseStream : this->BaseStreams) {
    assert(BaseStream->InnerMostLoop->contains(this->InnerMostLoop) &&
           "This should not happen for a candidate stream.");
    assert(BaseStream->isQualified() && "Can not check static mapping when "
                                        "base streams are not qualified yet.");
    if (BaseStream->InnerMostLoop == this->InnerMostLoop) {
      // With in the same inner most loop.
      continue;
    }
    // Parent loop base stream.
    auto BaseStpPattern = BaseStream->StaticStreamInfo.stp_pattern();
    if (BaseStpPattern != LLVM::TDG::StreamStepPattern::UNCONDITIONAL &&
        BaseStpPattern != LLVM::TDG::StreamStepPattern::NEVER) {
      // Illegal base stream step pattern.
      LLVM_DEBUG(llvm::dbgs()
                 << "Illegal BaseStream StepPattern " << BaseStpPattern
                 << " from " << BaseStream->formatName() << '\n');
      return false;
    }
    // Check my step pattern.
    if (MyStpPattern != LLVM::TDG::StreamStepPattern::UNCONDITIONAL &&
        MyStpPattern != LLVM::TDG::StreamStepPattern::NEVER) {
      return false;
    }
    // Most difficult part, check step count ratio is static.
    auto CurrentLoop = this->InnerMostLoop;
    while (CurrentLoop != BaseStream->InnerMostLoop) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Checking " << LoopUtils::getLoopId(CurrentLoop) << '\n');
      if (!this->SE->hasLoopInvariantBackedgeTakenCount(CurrentLoop)) {
        LLVM_DEBUG(llvm::dbgs() << "No loop invariant backedge count.\n");
        return false;
      }
      auto TripCountSCEV = LoopUtils::getTripCountSCEV(this->SE, CurrentLoop);
      if (llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV)) {
        LLVM_DEBUG(llvm::dbgs() << "No computable backedge count.\n");
        return false;
      }
      // The back edge should be invariant at ConfigureLoop.
      if (!this->SE->isLoopInvariant(TripCountSCEV, this->ConfigureLoop)) {
        LLVM_DEBUG(llvm::dbgs() << "No computable at configure loop.\n");
        return false;
      }
      /**
       * TODO: We should also check that this loop is guaranteed to entry.
       */
      CurrentLoop = CurrentLoop->getParentLoop();
      assert(CurrentLoop != nullptr && "Should have a parent loop.");
    }
  }
  return true;
}

void StaticStream::constructChosenGraph() {
  auto TranslateToChosen = [this](const StreamSet &BasicSet,
                                  StreamSet &ChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      if (!BaseS->isChosen()) {
        llvm::errs() << "Miss chosen stream " << BaseS->formatName() << " for "
                     << this->formatName() << ".\n";
        assert(false && "Missing chosen base stream.");
      }
      ChosenSet.insert(BaseS);
    }
  };
  auto TranslateToChosenNullable = [this](const StreamSet &BasicSet,
                                          StreamSet &ChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      if (!BaseS->isChosen()) {
        continue;
      }
      ChosenSet.insert(BaseS);
    }
  };
  assert(this->isChosen() && "Must be chosen to have ChosenGraph.");
  // Also for the other types.
  TranslateToChosen(this->BaseStreams, this->ChosenBaseStreams);
  // DependentStream may not be chosen
  TranslateToChosenNullable(this->DependentStreams,
                            this->ChosenDependentStreams);
  TranslateToChosen(this->BackMemBaseStreams, this->ChosenBackMemBaseStreams);
  // BackIVDependentStream may not be chosen
  TranslateToChosenNullable(this->BackIVDependentStreams,
                            this->ChosenBackIVDependentStreams);
  TranslateToChosen(this->BaseStepStreams, this->ChosenBaseStepStreams);
  TranslateToChosen(this->BaseStepRootStreams, this->ChosenBaseStepRootStreams);
}

std::string StaticStream::formatName() const {
  // We need a more compact encoding of a stream name. Since the function is
  // always the same, let it be (function line loop_header_bb inst_bb
  // inst_name)

  auto Line = 0;
  const auto &DebugLoc = this->Inst->getDebugLoc();
  if (DebugLoc) {
    Line = DebugLoc.getLine();
  }
  std::stringstream SS;
  SS << "(" << Utils::formatLLVMFunc(this->Inst->getFunction()) << " " << Line
     << " " << this->ConfigureLoop->getHeader()->getName().str() << " "
     << Utils::formatLLVMInstWithoutFunc(this->Inst) << ")";
  return SS.str();
}

void StaticStream::analyzeIsConditionalAccess() const {
  auto MyBB = this->Inst->getParent();
  for (auto Loop = this->InnerMostLoop; this->ConfigureLoop->contains(Loop);
       Loop = Loop->getParentLoop()) {
    auto HeadBB = Loop->getHeader();
    if (!this->PDT->dominates(MyBB, HeadBB)) {
      // This is conditional.
      this->StaticStreamInfo.set_is_cond_access(true);
      return;
    }
  }
  this->StaticStreamInfo.set_is_cond_access(false);
  return;
}

void StaticStream::analyzeIsTripCountFixed() const {
  for (auto Loop = this->InnerMostLoop; this->ConfigureLoop->contains(Loop);
       Loop = Loop->getParentLoop()) {
    auto TripCountSCEV = LoopUtils::getTripCountSCEV(this->SE, Loop);
    if (!llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV) &&
        this->SE->isLoopInvariant(TripCountSCEV, this->ConfigureLoop)) {
      continue;
    } else {
      this->StaticStreamInfo.set_is_trip_count_fixed(false);
      return;
    }
  }
  this->StaticStreamInfo.set_is_trip_count_fixed(true);
  return;
}

void StaticStream::generateAddrFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  if (this->AddrDG) {
    this->AddrDG->generateFunction(this->FuncNameBase + "_addr", Module);
  }
}

void StaticStream::generateReduceFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  if (this->ReduceDG) {
    this->ReduceDG->generateFunction(this->FuncNameBase + "_reduce", Module);
  }
}

void StaticStream::generateValueFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  if (this->ValueDG) {
    // Special case for fused load ops.
    if (this->Inst->getOpcode() == llvm::Instruction::Load &&
        !this->FusedLoadOps.empty()) {
      this->ValueDG->generateFunction(this->FuncNameBase + "_load", Module);
      assert(!this->UpdateStream && "FusedLoadOp in UpdateStream.");
      return;
    }
    this->ValueDG->generateFunctionWithFusedOp(
        this->getStoreFuncName(false), Module, this->LoadStoreBaseStreams,
        false /* IsLoad */);
    if (this->ValueDG->hasTailAtomicInst()) {
      // AtomicStream has one additional load func.
      this->ValueDG->generateFunctionWithFusedOp(
          this->getStoreFuncName(true), Module, this->LoadStoreBaseStreams,
          true /* IsLoad */);
    }
  }
  // Special case for update stream. The store stream will be chosen,
  // so we have to call it directly.
  if (this->Inst->getOpcode() == llvm::Instruction::Load &&
      this->UpdateStream &&
      this->StaticStreamInfo.compute_info().enabled_store_func()) {
    assert(this->UpdateStream->ValueDG && "Missing ValueDG for UpdateStream.");
    this->UpdateStream->generateValueFunction(Module);
  }
}

bool StaticStream::ComputeMetaNode::isEmpty() const {
  /**
   * Check if this ComputeMNode does nothing.
   * So far we just check that there is no compute insts.
   * Further we can allow something like binary extension.
   */
  if (!this->ComputeInsts.empty()) {
    return false;
  }
  // If this is a constant value, this is not empty.
  if (llvm::isa<llvm::ConstantData>(this->RootValue)) {
    return false;
  }
  return true;
}

bool StaticStream::ComputeMetaNode::isIdenticalTo(
    const ComputeMetaNode *Other) const {
  /**
   * Check if I am the same as the other compute node.
   */
  if (Other == this) {
    return true;
  }
  if (Other->ComputeInsts.size() != this->ComputeInsts.size()) {
    return false;
  }
  for (size_t ComputeInstIdx = 0, NumComputeInsts = this->ComputeInsts.size();
       ComputeInstIdx != NumComputeInsts; ++ComputeInstIdx) {
    const auto &ThisComputeInst = this->ComputeInsts.at(ComputeInstIdx);
    const auto &OtherComputeInst = Other->ComputeInsts.at(ComputeInstIdx);
    if (ThisComputeInst->getOpcode() != OtherComputeInst->getOpcode()) {
      return false;
    }
  }
  // We need to be more careful to check the inputs.
  if (this->LoadBaseStreams != Other->LoadBaseStreams) {
    return false;
  }
  if (this->CallInputs != Other->CallInputs) {
    return false;
  }
  if (this->LoopInvariantInputs != Other->LoopInvariantInputs) {
    return false;
  }
  if (this->IndVarBaseStreams != Other->IndVarBaseStreams) {
    return false;
  }

  return true;
}

void StaticStream::fuseLoadOps() {
  if (!StreamPassEnableFuseLoadOp) {
    return;
  }
  // So far only use this for the AtomicCmpXchg/Load
  if (this->Inst->getOpcode() != llvm::Instruction::AtomicCmpXchg &&
      this->Inst->getOpcode() != llvm::Instruction::Load) {
    return;
  }
  LLVM_DEBUG(llvm::dbgs() << "==== FuseLoadOps for " << this->formatName()
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
   * During this process, we mark candidate final value if the current frontier
   * has only one instruction and the current fused instructions has no outside
   * users (except the one in the frontier).
   *
   * Terminate when the frontier is empty.
   */

  // Helper fundtion to check all users are instructions from the same BB.
  auto AreUserInstsFromSameBB = [this](const llvm::Instruction *Inst) -> bool {
    for (auto User : Inst->users()) {
      auto UserInst = llvm::dyn_cast<llvm::Instruction>(User);
      if (!UserInst) {
        // User is not even an instruction.
        return false;
      }
      if (UserInst->getParent() != this->Inst->getParent()) {
        return false;
      }
    }
    return true;
  };

  if (!AreUserInstsFromSameBB(this->Inst)) {
    LLVM_DEBUG(llvm::dbgs()
               << "[FuseLoadOps] No fusing as user not in the same BB.\n");
    return;
  }

  InstVec FusedOps;
  InstSet FusedOpsSet;
  InstSet Frontier;
  InstSet NextFrontier;

  // Helper function to check all operands are used.
  auto AreUsedInstsFusedOrInvariant =
      [this, &FusedOpsSet](const llvm::Instruction *Inst) -> bool {
    for (auto OperandIdx = 0; OperandIdx < Inst->getNumOperands();
         ++OperandIdx) {
      auto Operand = Inst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(Operand)) {
        if (!this->ConfigureLoop->contains(OperandInst)) {
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
  auto HasDataSizeLEMine = [this](const llvm::Instruction *Inst) -> bool {
    return this->DataLayout->getTypeStoreSize(Inst->getType()) <=
           this->getMemElementSize();
  };

  FusedOps.push_back(this->Inst);
  FusedOpsSet.insert(this->Inst);
  Frontier.insert(this->Inst);
  while (!Frontier.empty()) {
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
      bool FormClosure = true;
      for (auto FusedInst : FusedOps) {
        bool AllUserFused = true;
        for (auto User : FusedInst->users()) {
          auto UserInst = llvm::dyn_cast<llvm::Instruction>(User);
          assert(UserInst && "User should always be an instruction.");
          if (!FusedOpsSet.count(UserInst) && UserInst != CandidateFinalInst) {
            AllUserFused = false;
            break;
          }
        }
        if (!AllUserFused) {
          FormClosure = false;
          break;
        }
      }
      if (FormClosure &&
          this->DataLayout->getTypeStoreSize(CandidateFinalInst->getType()) <
              this->getMemElementSize()) {
        // We found one closure.
        // Notice that we have to skip the first MyInst and add the
        // CandidateFinalInst
        this->FusedLoadOps.clear();
        this->FusedLoadOps.insert(this->FusedLoadOps.end(),
                                  FusedOps.begin() + 1, FusedOps.end());
        this->FusedLoadOps.push_back(CandidateFinalInst);
        LLVM_DEBUG({
          llvm::dbgs() << "[FuseLoadOps] ===== Found Closure.\n";
          for (auto Inst : this->FusedLoadOps) {
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

  if (this->FusedLoadOps.empty()) {
    return;
  }
  // Create the ValueDG if this is a LoadStream.
  // ValueDG for AtomicStream will be created by StaticStreamRegionAnalyzer.
  if (this->Inst->getOpcode() == llvm::Instruction::Load) {
    this->ValueDG = std::make_unique<StreamDataGraph>(
        this->ConfigureLoop, this->FusedLoadOps.back(),
        [](const llvm::PHINode *) -> bool { return false; });
  }
}

void StaticStream::fillProtobufStreamInfo(
    LLVM::TDG::StreamInfo *ProtobufInfo) const {
  auto ProtobufStaticInfo = ProtobufInfo->mutable_static_info();
  this->setStaticStreamInfo(*ProtobufStaticInfo);
  ProtobufInfo->set_name(this->formatName());
  ProtobufInfo->set_id(this->StreamId);
  ProtobufInfo->set_region_stream_id(this->RegionStreamId);
  switch (this->Inst->getOpcode()) {
  case llvm::Instruction::PHI:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_IV);
    break;
  case llvm::Instruction::Load:
  case llvm::Instruction::Call:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_LD);
    break;
  case llvm::Instruction::Store:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_ST);
    break;
  case llvm::Instruction::AtomicRMW:
  case llvm::Instruction::AtomicCmpXchg:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_AT);
    break;
  default:
    llvm::errs() << "Invalid stream type " << this->formatName() << '\n';
    break;
  }

  // Dump the address function.
  if (this->isChosen()) {
    auto AddrFuncInfo = ProtobufInfo->mutable_addr_func_info();
    this->fillProtobufAddrFuncInfo(AddrFuncInfo);
    // Dump the predication function.
    auto PredFuncInfo = ProtobufStaticInfo->mutable_pred_func_info();
    this->fillProtobufPredFuncInfo(PredFuncInfo);
    // Dump the store function.
    this->fillProtobufValueDGFuncInfo(ProtobufStaticInfo);
  }

  auto ProtobufCoalesceInfo = ProtobufInfo->mutable_coalesce_info();
  ProtobufCoalesceInfo->set_base_stream(this->CoalesceGroup);
  ProtobufCoalesceInfo->set_offset(this->CoalesceOffset);

  auto DynamicStreamInfo = ProtobufInfo->mutable_dynamic_info();
  DynamicStreamInfo->set_is_candidate(this->isCandidate());
  DynamicStreamInfo->set_is_qualified(this->isQualified());
  DynamicStreamInfo->set_is_chosen(this->isChosen());

#define ADD_STREAM(SET, FIELD)                                                 \
  {                                                                            \
    for (const auto &S : SET) {                                                \
      auto Entry = ProtobufInfo->add_##FIELD();                                \
      Entry->set_name(S->formatName());                                        \
      Entry->set_id(S->StreamId);                                              \
    }                                                                          \
  }
  ADD_STREAM(this->BaseStreams, base_streams);
  ADD_STREAM(this->BackMemBaseStreams, back_base_streams);
  ADD_STREAM(this->ChosenBaseStreams, chosen_base_streams);
  ADD_STREAM(this->ChosenBackMemBaseStreams, chosen_back_base_streams);

#undef ADD_STREAM
}

const StaticStream *
StaticStream::getExecFuncInputStream(const llvm::Value *Value) const {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    if (Inst == this->Inst) {
      // The input is myself. Only for PredFunc.
      return this;
    }
    for (auto BaseStream : this->ChosenBaseStreams) {
      if (BaseStream->Inst == Inst) {
        return BaseStream;
      }
    }
    if (this->ReduceDG) {
      for (auto BackBaseStream : this->ChosenBackMemBaseStreams) {
        if (BackBaseStream->Inst == Inst) {
          // The input is back base stream. Only for ReduceFunc.
          return BackBaseStream;
        }
        // Search for the induction variable of the BackBaseStream.
        for (auto BackBaseStepRootStream :
             BackBaseStream->ChosenBaseStepRootStreams) {
          if (BackBaseStepRootStream->Inst == Inst) {
            return BackBaseStepRootStream;
          }
        }
      }
    }
  }
  return nullptr;
}

void StaticStream::fillProtobufExecFuncInfo(
    ::LLVM::TDG::ExecFuncInfo *ProtoFuncInfo, const std::string &FuncName,
    const ExecutionDataGraph &ExecDG, llvm::Type *RetType) const {

  ProtoFuncInfo->set_name(FuncName);
  for (const auto &Input : ExecDG.getInputs()) {
    auto ProtobufArg = ProtoFuncInfo->add_args();
    auto Type = Input->getType();
    if (auto InputStream = this->getExecFuncInputStream(Input)) {
      // This comes from the base stream.
      ProtobufArg->set_is_stream(true);
      ProtobufArg->set_stream_id(InputStream->StreamId);
    } else {
      // This is an input value.
      ProtobufArg->set_is_stream(false);
    }
    ProtobufArg->set_type(this->translateToProtobufDataType(Type));
  }
  ProtoFuncInfo->set_type(this->translateToProtobufDataType(RetType));
}

void StaticStream::fillProtobufAddrFuncInfo(
    ::LLVM::TDG::ExecFuncInfo *AddrFuncInfo) const {

  assert(!(this->AddrDG && this->ReduceDG) &&
         "Can not have ReduceDG and AddrDG at the same time.");
  if (this->ReduceDG) {
    this->fillProtobufExecFuncInfo(AddrFuncInfo, this->FuncNameBase + "_reduce",
                                   *this->ReduceDG,
                                   this->ReduceDG->getReturnType(true));
  }

  if (this->AddrDG) {
    this->fillProtobufExecFuncInfo(AddrFuncInfo, this->FuncNameBase + "_addr",
                                   *this->AddrDG,
                                   this->AddrDG->getReturnType(true));
  }
}

void StaticStream::fillProtobufPredFuncInfo(
    ::LLVM::TDG::ExecFuncInfo *PredFuncInfo) const {
  if (this->BBPredDG) {
    this->fillProtobufExecFuncInfo(PredFuncInfo, this->BBPredDG->getFuncName(),
                                   *this->BBPredDG,
                                   this->BBPredDG->getReturnType(true));
  }
}

void StaticStream::fillProtobufValueDGFuncInfo(
    ::LLVM::TDG::StaticStreamInfo *SSInfo) const {

  if (!this->ValueDG) {
    return;
  }

  auto ProtoComputeInfo = SSInfo->mutable_compute_info();
  if (this->Inst->getOpcode() == llvm::Instruction::Load &&
      !this->FusedLoadOps.empty()) {
    // FusedLoadOps are treated as a LoadFunc.
    auto ProtoLoadInfo = ProtoComputeInfo->mutable_load_func_info();
    this->fillProtobufValueDGFuncInfoImpl(ProtoLoadInfo, true /* IsLoad */);
    return;
  }

  auto ProtoValueDGInfo = ProtoComputeInfo->mutable_store_func_info();
  this->fillProtobufValueDGFuncInfoImpl(ProtoValueDGInfo, false /* IsLoad */);

  if (this->ValueDG->hasTailAtomicInst()) {
    // AtomicStream has one additional load func.
    auto ProtoLoadInfo = ProtoComputeInfo->mutable_load_func_info();
    this->fillProtobufValueDGFuncInfoImpl(ProtoLoadInfo, true /* IsLoad */);
  }
}

void StaticStream::fillProtobufValueDGFuncInfoImpl(
    ::LLVM::TDG::ExecFuncInfo *ExInfo, bool IsLoad) const {

  ExInfo->set_name(this->getStoreFuncName(IsLoad));
  auto AddArg = [ExInfo, this](llvm::Type *Type,
                               const StaticStream *InputStream) -> void {
    auto ProtobufArg = ExInfo->add_args();
    if (InputStream) {
      // This comes from the base stream.
      ProtobufArg->set_is_stream(true);
      ProtobufArg->set_stream_id(InputStream->StreamId);
    } else {
      // This is an input value.
      ProtobufArg->set_is_stream(false);
    }
    ProtobufArg->set_type(this->translateToProtobufDataType(Type));
  };
  for (const auto &Input :
       this->ValueDG->getInputsWithFusedOp(this->LoadStoreBaseStreams)) {
    const StaticStream *InputStream = nullptr;
    // Search in the LoadStoreBaseStreams.
    for (auto IS : this->LoadStoreBaseStreams) {
      if (IS->getFinalFusedLoadInst() == Input) {
        InputStream = IS;
        break;
      }
    }
    // ! Be careful, ourself is still Inst, not FinalFusedLoadInst.
    if (this->Inst == Input) {
      InputStream = this;
    }
    AddArg(Input->getType(), InputStream);
  }

  // Finally handle tail AtomicRMWInst, with myself as the last input.
  auto TailAtomicInst = this->ValueDG->getTailAtomicInst();
  if (TailAtomicInst) {
    assert(TailAtomicInst == this->Inst && "Mismatch in TailAtomicInst.\n");
    auto AtomicInputPtrType = TailAtomicInst->getOperand(0)->getType();
    assert(AtomicInputPtrType->isPointerTy() && "Should be pointer.");
    AddArg(AtomicInputPtrType->getPointerElementType(), this);
  }

  auto StoreType = this->ValueDG->getReturnType(false /* IsLoad */);
  auto StoreTypeBits = DataLayout->getTypeStoreSizeInBits(StoreType);
  // if (StoreTypeBits > 64) {
  //   llvm::errs() << "Invalid result type, Stream: " << this->formatName()
  //                << " Bits " << StoreTypeBits << '\n';
  //   assert(false && "Invalid type for result.");
  // }
  ExInfo->set_type(this->translateToProtobufDataType(StoreType));
}

StaticStream::InputValueList StaticStream::getValueDGInputValues() const {
  /**
   * No special handling for atomic stream here, as load/store shares the same
   * input.
   */
  assert(this->ValueDG && "No ValueDG to get input values.");
  InputValueList InputValues;
  for (const auto &Input :
       this->ValueDG->getInputsWithFusedOp(this->LoadStoreBaseStreams)) {
    const StaticStream *InputStream = nullptr;
    // Search in the LoadStoreBaseStreams and myself.
    for (auto IS : this->LoadStoreBaseStreams) {
      if (IS->getFinalFusedLoadInst() == Input) {
        InputStream = IS;
        break;
      }
    }
    // ! Be careful, ourself is still Inst, not FinalFusedLoadInst.
    if (this->Inst == Input) {
      InputStream = this;
    }
    if (InputStream) {
      // This comes from the base stream at runtime.
    } else {
      // This is an input value.
      llvm::errs() << "Push in Input " << Utils::formatLLVMValue(Input)
                   << " Myself " << this->formatName() << '\n';
      InputValues.push_back(Input);
    }
  }
  return InputValues;
}

StaticStream::InputValueList
StaticStream::getExecFuncInputValues(const ExecutionDataGraph &ExecDG) const {
  InputValueList InputValues;
  for (const auto &Input : ExecDG.getInputs()) {
    if (auto InputStream = this->getExecFuncInputStream(Input)) {
      // This comes from the base stream.
    } else {
      // This is an input value.
      InputValues.push_back(Input);
    }
  }
  return InputValues;
}

StaticStream::InputValueList StaticStream::getReduceFuncInputValues() const {
  assert(this->isChosen() && "Only consider chosen stream's input values.");
  assert(this->ReduceDG && "No ReduceDG.");
  return this->getExecFuncInputValues(*this->ReduceDG);
}

StaticStream::InputValueList StaticStream::getPredFuncInputValues() const {
  assert(this->isChosen() && "Only consider chosen stream's input values.");
  assert(this->BBPredDG && "No BBPredDG.");
  assert(this->BBPredDG->isValid() && "Invalid BBPredDG.");
  return this->getExecFuncInputValues(*this->BBPredDG);
}