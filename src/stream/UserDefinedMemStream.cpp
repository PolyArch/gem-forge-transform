
#include "UserDefinedMemStream.h"

const std::unordered_set<std::string> UserDefinedMemStream::StreamLoadFuncNames{
    "ssp_load_i32",
};

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
        UserDefinedMemStream::StreamLoadFuncNames.count(Callee->getName()) !=
            0) {
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
