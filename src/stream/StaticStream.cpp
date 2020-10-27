#include "stream/StaticStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

#define DEBUG_TYPE "StaticStream"

uint64_t StaticStream::AllocatedStreamId = 0;

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
  auto CoreElementType = this->getCoreElementType();
  if (CoreElementType->isFloatTy()) {
    SSI.set_core_element_type(::LLVM::TDG::StreamDataType::FLOAT);
  } else if (CoreElementType->isDoubleTy()) {
    SSI.set_core_element_type(::LLVM::TDG::StreamDataType::DOUBLE);
  }
  auto MemElementType = this->getMemElementType();
  if (MemElementType->isFloatTy()) {
    SSI.set_mem_element_type(::LLVM::TDG::StreamDataType::FLOAT);
  } else if (MemElementType->isDoubleTy()) {
    SSI.set_mem_element_type(::LLVM::TDG::StreamDataType::DOUBLE);
  }
}

llvm::Type *StaticStream::getMemElementType() const {
  switch (this->Inst->getOpcode()) {
  case llvm::Instruction::PHI:
  case llvm::Instruction::Load:
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
            LLVM_DEBUG(llvm::errs()
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
      LLVM_DEBUG(llvm::errs() << "Pushing DFSNode "
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
    LLVM_DEBUG(llvm::errs()
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
      LLVM_DEBUG(llvm::errs()
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
      LLVM_DEBUG(llvm::errs()
                 << "Checking " << LoopUtils::getLoopId(CurrentLoop) << '\n');
      if (!this->SE->hasLoopInvariantBackedgeTakenCount(CurrentLoop)) {
        LLVM_DEBUG(llvm::errs() << "No loop invariant backedge count.\n");
        return false;
      }
      auto BackEdgeTakenSCEV = this->SE->getBackedgeTakenCount(CurrentLoop);
      if (llvm::isa<llvm::SCEVCouldNotCompute>(BackEdgeTakenSCEV)) {
        LLVM_DEBUG(llvm::errs() << "No computable backedge count.\n");
        return false;
      }
      // The back edge should be invariant at ConfigureLoop.
      if (!this->SE->isLoopInvariant(BackEdgeTakenSCEV, this->ConfigureLoop)) {
        LLVM_DEBUG(llvm::errs() << "No computable at configure loop.\n");
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
    auto BackEdgeTakenSCEV = this->SE->getBackedgeTakenCount(Loop);
    if (!llvm::isa<llvm::SCEVCouldNotCompute>(BackEdgeTakenSCEV) &&
        this->SE->isLoopInvariant(BackEdgeTakenSCEV, this->ConfigureLoop)) {
      continue;
    } else {
      this->StaticStreamInfo.set_is_trip_count_fixed(false);
      return;
    }
  }
  this->StaticStreamInfo.set_is_trip_count_fixed(true);
  return;
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
    this->ValueDG->generateFunction(this->getStoreFuncName(false), Module,
                                    false /* IsLoad */);
    if (this->ValueDG->hasTailAtomicInst()) {
      // AtomicStream has one additional load func.
      this->ValueDG->generateFunction(this->getStoreFuncName(true), Module,
                                      true /* IsLoad */);
    }
  }
  // Special case for update stream. The store store stream will be chosen,
  // so we have to call it directly.
  if (this->Inst->getOpcode() == llvm::Instruction::Load &&
      this->UpdateStream &&
      this->StaticStreamInfo.compute_info().enabled_store_func()) {
    assert(this->UpdateStream->ValueDG && "Missing ValueDG for UpdateStream.");
    this->UpdateStream->generateValueFunction(Module);
  }
}

void StaticStream::fillProtobufStoreFuncInfo(
    ::LLVM::TDG::StaticStreamInfo *SSInfo) const {

  if (!this->ValueDG) {
    return;
  }
  auto ProtoComputeInfo = SSInfo->mutable_compute_info();
  auto ProtoStoreInfo = ProtoComputeInfo->mutable_store_func_info();
  this->fillProtobufStoreFuncInfoImpl(ProtoStoreInfo, false /* IsLoad */);

  if (this->ValueDG->hasTailAtomicInst()) {
    // AtomicStream has one additional load func.
    auto ProtoLoadInfo = ProtoComputeInfo->mutable_load_func_info();
    this->fillProtobufStoreFuncInfoImpl(ProtoLoadInfo, true /* IsLoad */);
  }
}

void StaticStream::fillProtobufStoreFuncInfoImpl(
    ::LLVM::TDG::ExecFuncInfo *ExInfo, bool IsLoad) const {

  ExInfo->set_name(this->getStoreFuncName(IsLoad));
  auto CheckArg = [this](const llvm::Value *Input) -> void {
    auto Type = Input->getType();
    auto TypeSize = this->DataLayout->getTypeStoreSize(Type);
    if (TypeSize > 8) {
      llvm::errs() << "Invalid input type, Value: "
                   << Utils::formatLLVMValue(Input) << " TypeSize " << TypeSize
                   << '\n';
      assert(false && "Invalid type for input.");
    }
    if (!(Type->isIntOrPtrTy() || Type->isFloatTy() || Type->isDoubleTy())) {
      llvm::errs() << "Invalid input type, Value: "
                   << Utils::formatLLVMValue(Input) << " TypeSize " << TypeSize
                   << '\n';
      assert(false && "Invalid type for input.");
    }
  };
  auto AddArg = [ExInfo](const llvm::Type *Type,
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
    ProtobufArg->set_is_float(Type->isFloatTy() || Type->isDoubleTy());
  };
  for (const auto &Input : this->ValueDG->getInputs()) {
    StaticStream *InputStream = nullptr;
    // Search in the LoadStoreBaseStreams.
    for (auto IS : this->LoadStoreBaseStreams) {
      if (IS->Inst == Input) {
        InputStream = IS;
        break;
      }
    }
    CheckArg(Input);
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
  if (StoreTypeBits > 64) {
    llvm::errs() << "Invalid result type, Stream: " << this->formatName()
                 << " Bits " << StoreTypeBits << '\n';
    assert(false && "Invalid type for result.");
  }
  ExInfo->set_is_float(StoreType->isFloatTy() || StoreType->isDoubleTy());
}

StaticStream::InputValueList StaticStream::getStoreFuncInputValues() const {
  /**
   * No special handling for atomic stream here, as load/store shares the same
   * input.
   */
  assert(this->ValueDG && "No ValueDG to get input values.");
  InputValueList InputValues;
  for (const auto &Input : this->ValueDG->getInputs()) {
    StaticStream *InputStream = nullptr;
    // Search in the LoadStoreBaseStreams.
    for (auto IS : this->LoadStoreBaseStreams) {
      if (IS->Inst == Input) {
        InputStream = IS;
        break;
      }
    }
    if (InputStream) {
      // This comes from the base stream at runtime.
    } else {
      // This is an input value.
      InputValues.push_back(Input);
    }
  }
  return InputValues;
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
  // So far only use this for the AtomicCmpXchg stream.
  if (this->Inst->getOpcode() != llvm::Instruction::AtomicCmpXchg) {
    return;
  }
  LLVM_DEBUG(llvm::dbgs() << "==== FuseLoadOps for " << this->formatName()
                          << '\n');
  auto CurInst = this->Inst;
  while (CurInst->hasOneUse()) {
    auto User = *CurInst->user_begin();
    if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
      if (UserInst->getParent() == this->Inst->getParent()) {
        // We are in the same BB.
        // Todo: Make sure this is a single user chain.
        LLVM_DEBUG(llvm::dbgs() << "==== FuseLoadOp "
                                << Utils::formatLLVMInst(UserInst) << '\n');
        this->FusedLoadOps.push_back(UserInst);
        CurInst = UserInst;
        break;
      }
    }
  }
}