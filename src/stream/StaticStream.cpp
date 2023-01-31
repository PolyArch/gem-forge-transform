#include "stream/StaticStream.h"

#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

#define DEBUG_TYPE "StaticStream"

uint64_t StaticStream::AllocatedStreamId =
    ::LLVM::TDG::ReservedStreamRegionId::NumReservedStreamRegionId;

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
      SE(_SE), PDT(_PDT), DataLayout(_DataLayout), StreamName("Invalid"),
      IsCandidate(false), IsQualified(false), IsChosen(false),
      CoalesceGroup(StreamId), CoalesceOffset(0) {

  this->StreamName = this->generateStreamName();
  this->StaticStreamInfo.set_loop_level(this->InnerMostLoop->getLoopDepth());
  this->StaticStreamInfo.set_config_loop_level(
      this->ConfigureLoop->getLoopDepth());
  this->StaticStreamInfo.set_is_inner_most_loop(
      this->InnerMostLoop->getSubLoops().empty());
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
  llvm::errs() << "Invalid DataType: ";
  Type->print(llvm::errs());
  llvm::errs() << "\n";
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
  case llvm::Instruction::AtomicRMW:
    return this->Inst->getType();
  case llvm::Instruction::AtomicCmpXchg:
  case llvm::Instruction::Store: {
    auto AddrType = Utils::getMemAddrValue(this->Inst)->getType();
    auto ElementType = AddrType->getPointerElementType();
    return ElementType;
  }
  case llvm::Instruction::Call: {
    if (Utils::getCalledFunction(Inst)->getIntrinsicID() ==
        llvm::Intrinsic::masked_store) {
      auto AddrType = Utils::getMemAddrValue(this->Inst)->getType();
      auto ElementType = AddrType->getPointerElementType();
      return ElementType;
    } else {
      // This should be some atomic operation.
      return this->Inst->getType();
    }
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
      if (llvm::isa<llvm::LoadInst>(Inst) ||
          llvm::isa<llvm::AtomicRMWInst>(Inst)) {
        // LoadBaseStream.
        auto LoadBaseStream = GetStream(Inst, this->ConfigureLoop);
        if (!LoadBaseStream) {
          llvm::errs() << "Failed to find LoadBaseStream "
                       << Utils::formatLLVMInst(Inst) << " for "
                       << this->getStreamName() << '\n';
          assert(false && "Failed to find LoadBaseStream.");
        }
        if (this->UserParamInnerDep &&
            this->InnerMostLoop != LoadBaseStream->InnerMostLoop &&
            this->InnerMostLoop->contains(LoadBaseStream->InnerMostLoop)) {
          // Switch to InnerMostLoop stream.
          LoadBaseStream = GetStream(Inst, LoadBaseStream->InnerMostLoop);
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
          if (this->UserParamInnerDep &&
              this->InnerMostLoop != IndVarBaseStream->InnerMostLoop &&
              this->InnerMostLoop->contains(IndVarBaseStream->InnerMostLoop)) {
            // Switch to InnerMostLoop stream.
            IndVarBaseStream = GetStream(Inst, IndVarBaseStream->InnerMostLoop);
          }
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
                          << (Other ? Other->getStreamName() : "nullptr")
                          << '\n');
  this->BaseStreams.insert(Other);
  if (Other != nullptr) {
    Other->DependentStreams.insert(this);
    if (Other->InnerMostLoop == this->InnerMostLoop) {
      this->BaseStepStreams.insert(Other);
    }
  }
}

void StaticStream::addBackEdgeBaseStream(StaticStream *Other) {
  this->BackBaseStreams.insert(Other);
  Other->BackIVDependentStreams.insert(this);
}

void StaticStream::constructStreamGraph() {
  // Basically translate from what we found in LoadBaseStreams and
  // IndVarBaseStreams during the construction of the MetaGraph to BaseStreams
  // and BackBaseStreams.
  for (auto LoadBaseS : this->LoadBaseStreams) {
    if (LoadBaseS == this) {
      // No circle dependency in stream graph.
      continue;
    }
    if (LoadBaseS->InnerMostLoop == this->InnerMostLoop &&
        this->Type == TypeT::IV) {
      // This is a back edge dependence.
      this->addBackEdgeBaseStream(LoadBaseS);
    } else {
      this->addBaseStream(LoadBaseS);
    }
  }
  for (auto IndVarBaseS : this->IndVarBaseStreams) {
    if (IndVarBaseS == this) {
      // No circle dependency in stream graph.
      continue;
    }
    if (IndVarBaseS->InnerMostLoop == this->InnerMostLoop &&
        this->Type == TypeT::IV) {
      // This is a back edge from IV to IV.
      this->addBackEdgeBaseStream(IndVarBaseS);
    } else {
      this->addBaseStream(IndVarBaseS);
    }
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
                              << BaseStepRootStream->getStreamName() << '\n');
      this->BaseStepRootStreams.insert(BaseStepRootStream);
    }
  }
}

bool StaticStream::checkBaseStreamInnerMostLoopContainsMine() const {
  /**
   * We want to support getting the final value of inner-loop stream.
   * So far, we allow at most one inner-loop base stream.
   */
  const StaticStream *InnerBaseStream = nullptr;
  for (const auto &BaseStream : this->BaseStreams) {
    if (!BaseStream->InnerMostLoop->contains(this->InnerMostLoop)) {
      if (!InnerBaseStream) {
        InnerBaseStream = BaseStream;
      } else {
        return false;
      }
    }
  }
  for (const auto &BackBaseS : this->BackBaseStreams) {
    if (!BackBaseS->InnerMostLoop->contains(this->InnerMostLoop)) {
      if (!InnerBaseStream) {
        InnerBaseStream = BackBaseS;
      } else {
        return false;
      }
    }
  }
  return true;
}

bool StaticStream::checkStaticMapToBaseStreamsInParentLoop() const {
  // No need to worry about back edge base streams, cause they are guaranteed
  // to be not in a parent loop.
  assert(this->isCandidate() &&
         "Should not check static mapping for non-candidate stream.");
  for (const auto &BaseStream : this->BaseStreams) {
    assert(BaseStream->isQualified() && "Can not check static mapping when "
                                        "base streams are not qualified yet.");
    if (!BaseStream->InnerMostLoop->contains(this->InnerMostLoop)) {
      // This is an inner loop dependence, just ignore for now.
      continue;
    }
    if (BaseStream->InnerMostLoop == this->InnerMostLoop) {
      // With in the same inner most loop.
      continue;
    }
    // Parent loop base stream.
    if (!this->checkStaticMapToStreamInParentLoop(BaseStream)) {
      return false;
    }
  }
  return true;
}

bool StaticStream::checkStaticMapToStreamInParentLoop(
    const StaticStream *BaseStream) const {
  // Parent loop base stream.
  auto BaseStpPattern = BaseStream->StaticStreamInfo.stp_pattern();
  if (BaseStpPattern != LLVM::TDG::StreamStepPattern::UNCONDITIONAL &&
      BaseStpPattern != LLVM::TDG::StreamStepPattern::NEVER) {
    // Illegal base stream step pattern.
    LLVM_DEBUG(llvm::dbgs()
               << "Illegal BaseStream StepPattern " << BaseStpPattern
               << " from " << BaseStream->getStreamName() << '\n');
    return false;
  }
  // Check my step pattern.
  // NOTE: My StepPattern may not be set until I am marked qualifed.
  auto MyStpPattern = this->computeStepPattern();
  if (MyStpPattern != LLVM::TDG::StreamStepPattern::UNCONDITIONAL &&
      MyStpPattern != LLVM::TDG::StreamStepPattern::NEVER) {
    return false;
  }
  // Most difficult part, check step count ratio is static.
  if (!LoopUtils::hasLoopInvariantTripCountBetween(
          this->SE, this->ConfigureLoop, this->InnerMostLoop,
          BaseStream->InnerMostLoop)) {
    return false;
  }
  return true;
}

void StaticStream::constructChosenGraph() {
  auto TranslateToChosen = [this](const StreamSet &BasicSet,
                                  StreamSet &ChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      if (!BaseS->isChosen()) {
        llvm::errs() << "Miss chosen stream " << BaseS->getStreamName()
                     << " for " << this->getStreamName() << ".\n";
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
  TranslateToChosen(this->BackBaseStreams, this->ChosenBackBaseStreams);
  // BackIVDependentStream may not be chosen
  TranslateToChosenNullable(this->BackIVDependentStreams,
                            this->ChosenBackIVDependentStreams);
  TranslateToChosen(this->BaseStepStreams, this->ChosenBaseStepStreams);
  TranslateToChosen(this->BaseStepRootStreams, this->ChosenBaseStepRootStreams);
}

std::string
StaticStream::generateStreamNameFromMetaInfo(llvm::StringRef SSName) {

  /**
   * The name format is: name[/param]*
   * Anything after a slash '/' will some flags to guide the compiler.
   * TODO: Avoid using this feature too much.
   * List of flags:
   *  - inner-dep: When building the MetaGraph, choose the BaseStream
   *      with ConfigureLoop == InnerMostLoop. This breaks the default
   *      behavior to pick ConfigureLoop == DepStream's ConfigureLoop,
   *      and is a temporary solution to support an OuterLoopStream
   *      using the InnerLoopStream's exit value, when the
   *      InnerLoopStream can not be configured at outer loop.
   *  - non-spec: This stream should be executed non-speculatively,
   *      e.g., not offloaded until committed.
   *  - ld-cmp=group_id: Specify the group of LoadCompute input streams.
   *  - ld-cmp-result: This is the stream with LoadComputeResult.
   *  - nest=loop-level: Specify how many loop level this stream should be
   *      configured at, e.g., 2 will configure the stream at the second outer
   *      loop counting from the inner-most loop.
   */
  auto SlashPos = SSName.find('/');
  auto FirstSlashPos = SlashPos;
  while (SlashPos != SSName.npos) {
    auto NextSlashPos = SSName.find('/', SlashPos + 1);
    auto Param = SSName.substr(SlashPos + 1, NextSlashPos).str();
    this->StaticStreamInfo.add_user_param(Param);

    auto EqualPos = Param.find('=');
    auto ParamName = Param.substr(0, EqualPos);

    if (ParamName == "inner-dep") {
      this->UserParamInnerDep = true;
    } else if (ParamName == "ld-cmp") {
      assert(EqualPos != Param.npos && "Missing LoadCompute GroupIdx.");
      this->UserLoadComputeGroupIdx = std::stoi(Param.substr(EqualPos + 1));
    } else if (ParamName == "ld-cmp-result") {
      assert(this->UserLoadComputeGroupIdx != UserInvalidLoadComputeGroupIdx);
      this->UserLoadComputeResult = true;
    } else if (ParamName == "nest") {
      assert(EqualPos != Param.npos && "Missing NestLoopLevel GroupIdx.");
      this->UserNestLoopLevel = std::stoi(Param.substr(EqualPos + 1));
    } else if (ParamName == "no-stream") {
      this->UserNoStream = true;
    }

    SlashPos = NextSlashPos;
  }
  auto Ret = SSName.substr(0, FirstSlashPos).str();

  // Append the epilogue suffix.
  Ret = (this->ConfigureLoop->getHeader()->getName() + "-").str() + Ret;
  if (LoopUtils::isLoopRemainderOrEpilogue(this->InnerMostLoop)) {
    Ret += "-ep";
  }

  return Ret;
}

std::string StaticStream::generateStreamName() {

  /**
   * Prefer pragma specified name in #pragma ss stream_name "name".
   */
  if (auto SSMetadata = this->Inst->getMetadata("llvm.ss")) {
    for (unsigned OpIdx = 0, NumOps = SSMetadata->getNumOperands();
         OpIdx != NumOps; ++OpIdx) {
      const llvm::MDOperand &Op = SSMetadata->getOperand(OpIdx);
      if (auto SSField = llvm::dyn_cast<llvm::MDString>(Op.get())) {
        if (SSField->getString() == "ss.stream_name") {
          // Found ss.stream_name.
          if (OpIdx + 1 == NumOps) {
            llvm::errs()
                << "Found Metadata ss.stream_name, but no StreamName for Inst "
                << Utils::formatLLVMInst(this->Inst) << '\n';
            assert(false && "Missing StreamName.");
          }
          auto SSName = llvm::dyn_cast<llvm::MDString>(
              SSMetadata->getOperand(OpIdx + 1).get());
          assert(SSName && "StreamName should be MDString.");

          LLVM_DEBUG(llvm::dbgs() << "[StreamName] Using SS Pragma "
                                  << SSName->getString() << ".\n");
          return this->generateStreamNameFromMetaInfo(SSName->getString());
        }
      }
    }
  }

  /**
   * Otherwise, we need a more compact encoding of a stream name. Since the
   * function is always the same, let it be (function line loop_header_bb
   * inst_bb inst_name)
   */

  auto Line = 0;
  const auto &DebugLoc = this->Inst->getDebugLoc();
  if (DebugLoc) {
    Line = DebugLoc.getLine();
  }
  std::stringstream SS;
  SS << "(" << Utils::formatLLVMFunc(this->Inst->getFunction()) << " " << Line
     << " " << this->ConfigureLoop->getHeader()->getName().str()
     << (LoopUtils::isLoopRemainderOrEpilogue(this->InnerMostLoop) ? "-ep "
                                                                   : " ")
     << Utils::formatLLVMInstWithoutFunc(this->Inst) << ")";
  LLVM_DEBUG(llvm::dbgs() << "[StreamName] Using Generated Name " << SS.str()
                          << ".\n");
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
      LLVM_DEBUG({
        llvm::dbgs() << "Unknown TripCount ";
        this->SE->print(llvm::dbgs());
        llvm::dbgs() << '\n';
      });
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
    LLVM_DEBUG(llvm::dbgs()
               << "Generating AddrDG for " << this->getStreamName() << '\n');
    this->AddrDG->generateFunction(this->FuncNameBase + "_addr", Module);
  }
}

void StaticStream::generateReduceFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  if (this->ReduceDG) {
    LLVM_DEBUG(llvm::dbgs()
               << "Generating ReduceDG for " << this->getStreamName() << '\n');
    this->ReduceDG->generateFunction(this->FuncNameBase + "_reduce", Module);
  }
}

void StaticStream::generateValueFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  if (this->ValueDG) {
    LLVM_DEBUG(llvm::dbgs()
               << "Generating ValueDG for " << this->getStreamName() << '\n');
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
  // Special case for update stream. The store stream will be not chosen,
  // so we have to call it directly.
  if (this->Inst->getOpcode() == llvm::Instruction::Load &&
      this->UpdateStream &&
      this->StaticStreamInfo.compute_info().enabled_store_func()) {
    LLVM_DEBUG(llvm::dbgs()
               << "Generating UpdateDG for " << this->getStreamName() << '\n');
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
  // If this is a load, this is not empty.
  if (llvm::isa<llvm::LoadInst>(this->RootValue)) {
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

void StaticStream::fillProtobufStreamInfo(
    LLVM::TDG::StreamInfo *ProtobufInfo) const {
  auto ProtobufStaticInfo = ProtobufInfo->mutable_static_info();
  this->setStaticStreamInfo(*ProtobufStaticInfo);
  ProtobufInfo->set_name(this->getStreamName());
  ProtobufInfo->set_id(this->StreamId);
  ProtobufInfo->set_region_stream_id(this->RegionStreamId);
  switch (this->Inst->getOpcode()) {
  case llvm::Instruction::PHI:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_IV);
    break;
  case llvm::Instruction::Load:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_LD);
    break;
  case llvm::Instruction::Store:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_ST);
    break;
  case llvm::Instruction::AtomicRMW:
  case llvm::Instruction::AtomicCmpXchg:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_AT);
    break;
  case llvm::Instruction::Call: {
    if (Utils::getCalledFunction(Inst)->getIntrinsicID() ==
        llvm::Intrinsic::masked_store) {
      ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_ST);
    } else {
      ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_LD);
    }
    break;
  }
  default:
    llvm::errs() << "Invalid stream type " << this->getStreamName() << '\n';
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
      Entry->set_name(S->getStreamName());                                     \
      Entry->set_id(S->StreamId);                                              \
    }                                                                          \
  }
  ADD_STREAM(this->BaseStreams, base_streams);
  ADD_STREAM(this->BackBaseStreams, back_base_streams);
  ADD_STREAM(this->ChosenBaseStreams, chosen_base_streams);
  ADD_STREAM(this->ChosenBackBaseStreams, chosen_back_base_streams);

#undef ADD_STREAM
}

const StaticStream *
StaticStream::getExecFuncInputStream(const llvm::Value *Value) const {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    if (Inst == this->Inst) {
      // The input is myself. Only for PredFunc.
      return this;
    }
    if (!this->FusedLoadOps.empty() && Inst == this->FusedLoadOps.back()) {
      // The input is my fused load op. Only for PredFunc.
      return this;
    }
    for (auto BaseStream : this->ChosenBaseStreams) {
      if (BaseStream->Inst == Inst) {
        return BaseStream;
      }
    }
    if (this->ReduceDG) {
      for (auto BackBaseS : this->ChosenBackBaseStreams) {
        if (BackBaseS->Inst == Inst) {
          // The input is back base stream. Only for ReduceFunc.
          return BackBaseS;
        }
        // Search for the induction variable of the BackBaseStream.
        for (auto BackBaseStepRootS : BackBaseS->ChosenBaseStepRootStreams) {
          if (BackBaseStepRootS->Inst == Inst) {
            return BackBaseStepRootS;
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
  ProtoFuncInfo->set_compute_op(ExecDG.getComputeOp());
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
    const StaticStream *InputS = nullptr;
    // Search in the LoadStoreBaseStreams.
    for (auto S : this->LoadStoreBaseStreams) {
      if (S->getFinalFusedLoadInst() == Input) {
        InputS = S;
        break;
      }
      if (S->ReduceDG && S->ReduceDG->getSingleResultValue() == Input) {
        InputS = S;
        break;
      }
    }
    // ! Be careful, ourself is still Inst, not FinalFusedLoadInst.
    if (this->Inst == Input) {
      InputS = this;
    }
    AddArg(Input->getType(), InputS);
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
  //   llvm::errs() << "Invalid result type, Stream: " << this->getStreamName()
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
    const StaticStream *InputS = nullptr;
    // Search in the LoadStoreBaseStreams and myself.
    for (auto S : this->LoadStoreBaseStreams) {
      if (S->getFinalFusedLoadInst() == Input) {
        InputS = S;
        break;
      }
      if (S->ReduceDG && S->ReduceDG->getSingleResultValue() == Input) {
        InputS = S;
        break;
      }
    }
    // ! Be careful, ourself is still Inst, not FinalFusedLoadInst.
    if (this->Inst == Input) {
      InputS = this;
    }
    if (InputS) {
      // This comes from the base stream at runtime.
    } else {
      // This is an input value.
      llvm::errs() << "Push in Input " << Utils::formatLLVMValue(Input)
                   << " Myself " << this->getStreamName() << '\n';
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