#ifndef LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H
#define LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H

#include "stream/Stream.h"

#include <sstream>
class InductionVarStream : public Stream {
public:
  InductionVarStream(const std::string &_Folder, const llvm::PHINode *_PHIInst,
                     const llvm::Loop *_Loop, const llvm::Loop *_InnerMostLoop,
                     size_t _Level, llvm::DataLayout *DataLayout);
  InductionVarStream(const InductionVarStream &Other) = delete;
  InductionVarStream(InductionVarStream &&Other) = delete;
  InductionVarStream &operator=(const InductionVarStream &Other) = delete;
  InductionVarStream &operator=(InductionVarStream &&Other) = delete;

  void buildBasicDependenceGraph(GetStreamFuncT GetStream) override;
  void
  buildChosenDependenceGraph(GetChosenStreamFuncT GetChosenStream) override;

  const llvm::PHINode *getPHIInst() const { return this->PHIInst; }
  const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const override {
    return this->ComputeInsts;
  }
  const std::unordered_set<const llvm::LoadInst *> &
  getBaseLoads() const override {
    return this->BaseLoadInsts;
  }
  const std::unordered_set<const llvm::Instruction *> &
  getStepInsts() const override {
    return this->StepInsts;
  }

  bool isCandidate() const override;
  bool isQualifySeed() const override;

  void addAccess(const DynamicValue &DynamicVal) override {
    if (!this->IsCandidateStatic) {
      // I am not even a candidate, ignore all this.
      return;
    }
    auto Type = this->PHIInst->getType();
    if (Type->isIntegerTy()) {
      this->addAccess(DynamicVal.getInt());
    } else if (Type->isPointerTy()) {
      this->addAccess(DynamicVal.getAddr());
    } else {
      llvm_unreachable("Invalid type for induction variable stream.");
    }
  }

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
  std::unordered_set<const llvm::LoadInst *> BaseLoadInsts;
  std::unordered_set<const llvm::Instruction *> StepInsts;
  bool IsCandidateStatic;

  void addAccess(uint64_t Value) {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addAccess(Value);
      this->LastAccessIters = this->Iters;
      this->TotalAccesses++;
    }
  }

  /**
   * Do a BFS on the PHINode and extract all the compute instructions.
   */
  void searchComputeInsts(const llvm::PHINode *PHINode, const llvm::Loop *Loop);

  /**
   * Do a BFS on the PHINode and extract all the step instructions.
   */
  static std::unordered_set<const llvm::Instruction *>
  searchStepInsts(const llvm::PHINode *PHINode, const llvm::Loop *Loop);

  /**
   * Find the step instructions by looking at the possible in

  /**
   * A phi node is an static candidate induction variable stream if:
   * 1. It is of type integer.
   * 2. Its compute instructions or call/invoke.
   * 3. Contains at most one base load stream in the same inner most loop.
   */
  bool isCandidateStatic() const;
};
#endif