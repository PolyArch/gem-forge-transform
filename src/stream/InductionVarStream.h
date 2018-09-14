#ifndef LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H
#define LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H

#include "stream/Stream.h"

#include <sstream>
class InductionVarStream : public Stream {
public:
  InductionVarStream(
      const std::string &_Folder, const llvm::PHINode *_PHIInst,
      const llvm::Loop *_Loop, size_t _Level,
      std::unordered_set<const llvm::Instruction *> &&_ComputeInsts)
      : Stream(TypeT::IV, _Folder, _PHIInst, _Loop, _Level), PHIInst(_PHIInst),
        ComputeInsts(std::move(_ComputeInsts)) {}

  InductionVarStream(const InductionVarStream &Other) = delete;
  InductionVarStream(InductionVarStream &&Other) = delete;
  InductionVarStream &operator=(const InductionVarStream &Other) = delete;
  InductionVarStream &operator=(InductionVarStream &&Other) = delete;

  const llvm::PHINode *getPHIInst() const { return this->PHIInst; }
  const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const override {
    return this->ComputeInsts;
  }

  void addAccess(uint64_t Value) {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addAccess(Value);
      this->LastAccessIters = this->Iters;
      this->TotalAccesses++;
    }
  }

  void endStream() {
    this->Pattern.endStream();
    this->Iters = 1;
    this->LastAccessIters = 0;
    this->TotalStreams++;
  }

  /**
   * Do a BFS on the PHINode and extract all the compute instructions.
   */
  static std::unordered_set<const llvm::Instruction *>
  searchComputeInsts(const llvm::PHINode *PHINode, const llvm::Loop *Loop);

  /**
   * A phi node is an induction variable stream if:
   * 1. It is of type integer.
   * 2. Its compute instructions do not contain memory access or call/invoke.
   */
  static bool isInductionVarStream(
      const llvm::PHINode *PHINode,
      const std::unordered_set<const llvm::Instruction *> &ComputeInsts);


  std::string format() const {
    std::stringstream ss;
    ss << "InductionVarStream " << LoopUtils::formatLLVMInst(this->PHIInst)
       << '\n';

    ss << "ComputeInsts: ------\n";
    for (const auto &ComputeInst : this->ComputeInsts) {
      ss << LoopUtils::formatLLVMInst(ComputeInst) << '\n';
    }
    ss << "ComputeInsts: end---\n";

    return ss.str();
  }

private:
  const llvm::PHINode *PHIInst;
  std::unordered_set<const llvm::Instruction *> ComputeInsts;
};
#endif