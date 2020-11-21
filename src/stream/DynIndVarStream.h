#ifndef LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H
#define LLVM_TDG_STREAM_INDUCTION_VAR_STREAM_H

#include "stream/DynStream.h"

#include <sstream>
class DynIndVarStream : public DynStream {
public:
  using InstSet = StaticStream::InstSet;
  DynIndVarStream(const std::string &_Folder,
                  const std::string &_RelativeFolder, StaticStream *_SStream,
                  llvm::DataLayout *DataLayout);
  DynIndVarStream(const DynIndVarStream &Other) = delete;
  DynIndVarStream(DynIndVarStream &&Other) = delete;
  DynIndVarStream &operator=(const DynIndVarStream &Other) = delete;
  DynIndVarStream &operator=(DynIndVarStream &&Other) = delete;

  void buildBasicDependenceGraph(GetStreamFuncT GetStream) override;

  const llvm::PHINode *getPHIInst() const { return this->PHIInst; }
  const InstSet &getComputeInsts() const override { return this->ComputeInsts; }
  const std::unordered_set<const llvm::LoadInst *> &
  getBaseLoads() const override {
    return this->BaseLoadInsts;
  }
  const InstSet &getStepInsts() const override {
    return this->SStream->getStepInsts();
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
    ss << "DynIndVarStream " << Utils::formatLLVMInst(this->PHIInst) << '\n';

    ss << "ComputeInsts: ------\n";
    for (const auto &ComputeInst : this->ComputeInsts) {
      ss << Utils::formatLLVMInst(ComputeInst) << '\n';
    }
    ss << "ComputeInsts: end---\n";

    return ss.str();
  }

private:
  const llvm::PHINode *PHIInst;
  std::unordered_set<const llvm::Instruction *> ComputeInsts;
  std::unordered_set<const llvm::LoadInst *> BaseLoadInsts;
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
   * A phi node is an static candidate induction variable stream if:
   * 1. It is of type integer.
   * 2. Its compute instructions has no call/invoke.
   * 3. Contains at most one base load stream in the same inner most loop.
   */
  bool isCandidateStatic() const;

  /**
   * This is an initial attempt to analyze the stream pattern statically.
   */
  bool isStaticStream() const;
};
#endif