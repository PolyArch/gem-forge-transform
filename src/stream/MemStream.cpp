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

void MemStream::generateComputeFunction(
    std::unique_ptr<llvm::Module> &Module) const {
  this->AddrDG.generateComputeFunction(this->AddressFunctionName, Module);
}
