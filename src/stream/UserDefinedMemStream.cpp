
#include "UserDefinedMemStream.h"

const std::unordered_set<std::string> UserDefinedMemStream::StreamLoadFuncNames{
    "ssp_load_i32",
};

UserDefinedMemStream::UserDefinedMemStream(
    StaticStreamRegionAnalyzer *_Analyzer, llvm::Instruction *_Inst,
    const llvm::Loop *_ConfigureLoop, const llvm::Loop *_InnerMostLoop,
    llvm::ScalarEvolution *_SE, const llvm::PostDominatorTree *_PDT,
    llvm::DataLayout *_DataLayout)
    : StaticStream(_Analyzer, TypeT::USER, _Inst, _ConfigureLoop,
                   _InnerMostLoop, _SE, _PDT, _DataLayout),
      UserInst(nullptr), ConfigInst(nullptr), EndInst(nullptr) {
  assert(isUserDefinedMemStream(this->Inst) &&
         "This is not a UserDefinedMemStream.");
  this->searchUserDefinedControlInstructions();
}

void UserDefinedMemStream::finalizePattern() {}

/**
 * Check if this is a user-defined memory stream.
 * Basically this is a call to ssp_stream_load_xx.
 */
bool UserDefinedMemStream::isUserDefinedMemStream(
    const llvm::Instruction *Inst) {
  if (Utils::isCallOrInvokeInst(Inst)) {
    auto *Callee = Utils::getCalledFunction(Inst);
    if (Callee->isDeclaration() &&
        UserDefinedMemStream::StreamLoadFuncNames.count(
            Callee->getName().str()) != 0) {
      return true;
    }
  }
  return false;
}

/**
 * Initialize the DFSStack by pushing the first ComputeMetaGraph.
 */
void UserDefinedMemStream::initializeMetaGraphConstruction(
    std::list<DFSNode> &DFSStack,
    ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
    ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) {}

LLVM::TDG::StreamStepPattern UserDefinedMemStream::computeStepPattern() const {
  return LLVM::TDG::StreamStepPattern::CONDITIONAL;
}

void UserDefinedMemStream::searchUserDefinedControlInstructions() {
  this->UserInst = llvm::dyn_cast<llvm::CallInst>(this->Inst);
  assert(this->UserInst && "UserInst should be CallInst.");
  this->ConfigInst =
      llvm::dyn_cast<llvm::CallInst>(this->UserInst->getArgOperand(0));
  assert(this->ConfigInst && "ConfigInst should be CallInst.");

  // Get user of the ConfigInst.
  for (auto ConfigUser : this->ConfigInst->users()) {
    auto ConfigUserCallInst = llvm::dyn_cast<llvm::CallInst>(ConfigUser);
    assert(ConfigUserCallInst && "ConfigUser should also be CallInst.");
    auto *Callee = Utils::getCalledFunction(ConfigUserCallInst);
    if (Callee->getName() == "ssp_end") {
      assert(!this->EndInst && "Multple EndInst.");
      this->EndInst = ConfigUserCallInst;
    } else if (Callee->getName() == "ssp_step") {
      this->StepInsts.insert(ConfigUserCallInst);
    }
  }

  /**
   * Simple sanity check.
   */
  assert(this->EndInst && "Missing EndInst.");
  assert(!this->StepInsts.empty() && "Missing StepInst.");
}

void UserDefinedMemStream::analyzeIsCandidate() {
  /**
   * I will take the highest level of the loop that
   * does not contain my ConfigInst.
   */
  if (auto ParentLoop = this->ConfigureLoop->getParentLoop()) {
    if (!ParentLoop->contains(this->ConfigInst->getParent())) {
      // This is not the outer-most loop not containing my ConfigInst.
      this->IsCandidate = false;
      this->StaticStreamInfo.set_not_stream_reason(
          LLVM::TDG::StaticStreamInfo::USER_NOT_OUT_MOST);
      return;
    }
  }
  this->StaticStreamInfo.set_val_pattern(LLVM::TDG::StreamValuePattern::LINEAR);
  this->StaticStreamInfo.set_stp_pattern(
      LLVM::TDG::StreamStepPattern::UNCONDITIONAL);
  this->IsCandidate = true;
}