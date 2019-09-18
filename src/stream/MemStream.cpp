#include "stream/MemStream.h"

llvm::cl::opt<bool> StreamPassAllowAliasedStream(
    "stream-pass-allow-aliased-stream",
    llvm::cl::desc("Allow aliased streams to be specialized."));

void MemStream::searchAddressComputeInstructions(
    std::function<bool(const llvm::PHINode *)> IsInductionVar) {
  std::list<llvm::Instruction *> Queue;

  llvm::Value *AddrValue = nullptr;
  if (llvm::isa<llvm::LoadInst>(this->getInst())) {
    AddrValue = this->getInst()->getOperand(0);
  } else {
    AddrValue = this->getInst()->getOperand(1);
  }

  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(AddrValue)) {
    Queue.emplace_back(AddrInst);
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    if (this->AddrInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      continue;
    }
    if (!this->getLoop()->contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    if (auto BaseLoad = llvm::dyn_cast<llvm::LoadInst>(CurrentInst)) {
      // This is also a base load.
      this->BaseLoads.insert(BaseLoad);
      // Do not go further for load.
      continue;
    }

    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(CurrentInst)) {
      if (IsInductionVar(PHINode)) {
        this->BaseInductionVars.insert(PHINode);
        // Do not go further for induction variables.
        continue;
      }
    }

    this->AddrInsts.insert(CurrentInst);
    // BFS on the operands.
    for (unsigned OperandIdx = 0, NumOperands = CurrentInst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        // This is an instruction within the same context.
        Queue.emplace_back(OperandInst);
      }
    }
  }
}

void MemStream::buildBasicDependenceGraph(GetStreamFuncT GetStream) {
  for (const auto &BaseIV : this->BaseInductionVars) {
    auto BaseIVStream = GetStream(BaseIV, this->getLoop());
    assert(BaseIVStream != nullptr && "Missing base IVStream.");
    this->addBaseStream(BaseIVStream);
  }
  for (const auto &BaseLoad : this->BaseLoads) {
    auto BaseMStream = GetStream(BaseLoad, this->getLoop());
    assert(BaseMStream != nullptr && "Missing base MemStream.");
    this->addBaseStream(BaseMStream);
  }
}

bool MemStream::isCandidate() const {
  // I have to contain no circle to be a candidate.
  if (this->AddrDG.hasCircle()) {
    return false;
  }
  if (this->AddrDG.hasPHINodeInComputeInsts()) {
    return false;
  }
  if (this->AddrDG.hasCallInstInComputeInsts()) {
    return false;
  }
  // Also ignore myself if I have no accesses.
  if (this->TotalAccesses == 0) {
    return false;
  }
  if (this->TotalIters < 10) {
    // Such a short stream.
    return false;
  }
  // Check whitelist.
  {
    auto StreamWhitelist = StreamUtils::getStreamWhitelist();
    if (StreamWhitelist.isInitialized()) {
      if (!StreamWhitelist.contains(this->SStream->Inst)) {
        return false;
      }
    }
  }
  /**
   * Hack to reduce number of streams.
   */
  auto AverageItersPerConfig = static_cast<double>(this->TotalIters) /
                               static_cast<double>(this->TotalStreams);

  if (AverageItersPerConfig < 25) {
    /**
     * ! Manually hack to disable some regions as there are too many streams.
     */
    llvm::errs() << AverageItersPerConfig << " LoopId for short streams "
                 << LoopUtils::getLoopId(this->SStream->ConfigureLoop) << '\n';
    if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
        // ! gcc_s
        "et-forest.c::302(et_splay)::bb10") {
      return false;
    }
    if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
        "SystemMatrices.c::285(CalculateA)::bb5689") {
      return false;
    }
    if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
        "SystemMatrices.c::285(CalculateA)::bb5980") {
      return false;
    }
    if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
        "lattice.c::427(calc_latt_deform)::bb206") {
      // ! blender_r
      // ! Do not allow configure at this level, force configuration at inner
      // loop.
      return false;
    }
    if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
        "lattice.c::427(calc_latt_deform)::bb239") {
      // ! blender_r
      // ! Do not allow configure at this level, force configuration at inner
      // loop.
      return false;
    }
  }
  // ! parest_r
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "forward.cc::25(void "
      "METomography::ForwardSolver::block_build_matrix_and_rhs_threaded<3>)::"
      "bb624") {
    return false;
  }
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "dof_handler.cc::1952(dealii::DoFHandler<3, "
      "3>::distribute_dofs)::bb418") {
    return false;
  }
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "forward.cc::25(void "
      "METomography::ForwardSolver::block_build_matrix_and_rhs_threaded<3>)::"
      "bb630") {
    return false;
  }
  // ! leela_s
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "_ZN9FastState16play_random_moveEv::bb1002") {
    return false;
  }
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "_ZN9FastState16play_random_moveEv::bb189") {
    return false;
  }
  // ! mser
  if (LoopUtils::getLoopId(this->SStream->ConfigureLoop) ==
      "mser.c::68(mser)::bb233") {
    return false;
  }
  // ! liblinear.
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "linear.c::844(solve_l2r_l1l2_svc)::bb218") {
    if (this->SStream->InnerMostLoop != this->SStream->ConfigureLoop) {
      return false;
    }
  }
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "linear.c::844(solve_l2r_l1l2_svc)::bb295") {
    if (this->SStream->InnerMostLoop != this->SStream->ConfigureLoop) {
      return false;
    }
  }
  // ! srr.
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "SystemMatrices.c::285(CalculateA)::bb2733") {
    return false;
  }
  if (LoopUtils::getLoopId(this->SStream->InnerMostLoop) ==
      "SystemMatrices.c::285(CalculateA)::bb4210") {
    return false;
  }

  if (this->BaseStepRootStreams.size() > 1) {
    // More than 1 step streams.
    return false;
  }
  return true;
}

bool MemStream::isQualifySeed() const {
  if (!this->isCandidate()) {
    return false;
  }
  if (this->isAliased() && (!StreamPassAllowAliasedStream)) {
    return false;
  }
  if (this->BaseStreams.empty()) {
    if (!this->Pattern.computed()) {
      return false;
    }
    auto AddrPattern = this->Pattern.getPattern().ValPattern;
    if (AddrPattern <= StreamPattern::ValuePattern::QUARDRIC) {
      // This is affine. Push into the queue as start point.
      return true;
    }
    return false;
  } else {
    // Check all the base streams.
    for (const auto &BaseStream : this->BaseStreams) {
      if (!BaseStream->isQualified()) {
        return false;
      }
    }
    return true;
  }
}

void MemStream::formatAdditionalInfoText(std::ostream &OStream) const {
  this->AddrDG.format(OStream);
}

std::list<const llvm::Value *> MemStream::getInputValues() const {
  if (this->SStream->inputValuesValid) {
    return this->SStream->inputValues;
  }

  assert(this->isChosen() && "Only consider chosen stream's input values.");
  std::list<const llvm::Value *> InputValues;
  auto FindBaseStream = [this](const llvm::Value *Value) -> Stream * {
    if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
      for (auto BaseStream : this->getChosenBaseStreams()) {
        if (BaseStream->SStream->Inst == Inst) {
          return BaseStream;
        }
      }
    }
    return nullptr;
  };

  for (const auto &Input : this->AddrDG.getInputs()) {
    if (auto BaseStream = FindBaseStream(Input)) {
      // This comes from the base stream.
    } else {
      // This is an input value.
      InputValues.push_back(Input);
    }
  }
  return InputValues;
}

void MemStream::fillProtobufAddrFuncInfo(
    ::llvm::DataLayout *DataLayout,
    ::LLVM::TDG::StreamInfo *ProtobufInfo) const {

  if (!this->isChosen()) {
    return;
  }

  auto AddrFuncInfo = ProtobufInfo->mutable_addr_func_info();
  AddrFuncInfo->set_name(this->AddressFunctionName);

  auto FindBaseStream = [this](const llvm::Value *Value) -> Stream * {
    if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
      for (auto BaseStream : this->getChosenBaseStreams()) {
        if (BaseStream->SStream->Inst == Inst) {
          return BaseStream;
        }
      }
    }
    return nullptr;
  };

  for (const auto &Input : this->AddrDG.getInputs()) {
    auto ProtobufArg = AddrFuncInfo->add_args();
    if (auto BaseStream = FindBaseStream(Input)) {
      // This comes from the base stream.
      ProtobufArg->set_is_stream(true);
      ProtobufArg->set_stream_id(BaseStream->getStreamId());
    } else {
      // This is an input value.
      ProtobufArg->set_is_stream(false);
    }
  }
}

void MemStream::generateComputeFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  this->AddrDG.generateComputeFunction(this->AddressFunctionName, Module);
}
