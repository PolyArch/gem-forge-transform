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
      Entry->mutable_id()->set_name(S->formatName());                          \
      Entry->mutable_id()->set_id(S->StreamId);                                \
      Entry->set_pred_true(PRED_TRUE);                                         \
    }                                                                          \
  }
  ADD_STREAM(this->PredicatedTrueStreams, predicated_streams, true);
  ADD_STREAM(this->PredicatedFalseStreams, predicated_streams, false);
#undef ADD_STREAM
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
        assert(LoadBaseStream != nullptr && "Failed to find LoadBaseStream.");
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
  LLVM_DEBUG(llvm::errs() << "Construct metagraph done.\n");
  this->constructStreamGraph();
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
    this->ReduceDG->generateComputeFunction(this->FuncNameBase + "_reduce",
                                            Module);
  }
  if (this->StoreDG) {
    this->StoreDG->generateComputeFunction(this->FuncNameBase + "_store",
                                           Module);
  }
}

void StaticStream::fillProtobufStoreFuncInfo(
    ::llvm::DataLayout *DataLayout,
    ::LLVM::TDG::ExecFuncInfo *ProtoFuncInfo) const {

  if (!this->StoreDG) {
    return;
  }
  ProtoFuncInfo->set_name(this->FuncNameBase + "_store");
  for (const auto &Input : this->StoreDG->getInputs()) {
    auto ProtobufArg = ProtoFuncInfo->add_args();
    auto Type = Input->getType();
    auto TypeSize = DataLayout->getTypeStoreSize(Type);
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
    StaticStream *InputStream = nullptr;
    // Search in the LoadStoreBaseStreams.
    for (auto IS : this->LoadStoreBaseStreams) {
      if (IS->Inst == Input) {
        InputStream = IS;
        break;
      }
    }
    if (InputStream) {
      // This comes from the base stream.
      ProtobufArg->set_is_stream(true);
      ProtobufArg->set_stream_id(InputStream->StreamId);
    } else {
      // This is an input value.
      ProtobufArg->set_is_stream(false);
    }
    ProtobufArg->set_is_float(Type->isFloatTy() || Type->isDoubleTy());
  }
  auto StoreValue = this->StoreDG->getResultValue();
  auto StoreType = StoreValue->getType();
  if (!(StoreType->isIntOrPtrTy() || StoreType->isFloatTy() ||
        StoreType->isDoubleTy())) {
    llvm::errs() << "Invalid result type, Value: "
                 << Utils::formatLLVMValue(StoreValue) << '\n';
    assert(false && "Invalid type for result.");
  }
  ProtoFuncInfo->set_is_float(StoreType->isFloatTy() ||
                              StoreType->isDoubleTy());
}

StaticStream::InputValueList StaticStream::getStoreFuncInputValues() const {
  assert(this->StoreDG && "No StoreDG to get input values.");
  InputValueList InputValues;
  for (const auto &Input : this->StoreDG->getInputs()) {
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