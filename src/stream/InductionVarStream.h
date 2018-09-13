#ifndef LLVM_TDG_INDUCTION_VAR_STREAM_H
#define LLVM_TDG_INDUCTION_VAR_STREAM_H

#include "LoopUtils.h"
#include "MemoryPattern.h"
#include "Utils.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"

#include <sstream>
#include <string>
#include <unordered_set>

class Stream {
public:
  enum TypeT {
    IV,
    MEM,
  };
  const TypeT Type;
  Stream(TypeT _Type)
      : Type(_Type), Qualified(false), TotalIters(0), TotalAccesses(0),
        TotalStreams(0), Iters(1), LastAccessIters(0), Pattern() {}

  const std::unordered_set<Stream *> &getBaseStreams() const {
    return this->BaseStreams;
  }
  const std::unordered_set<Stream *> &getDependentStreams() const {
    return this->DependentStreams;
  }
  void markQualified() { this->Qualified = true; }
  bool isQualified() const { return this->Qualified; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  const MemoryPattern &getPattern() const { return this->Pattern; }

  void addBaseStream(Stream *Other) {
    assert(Other != this && "Self dependent streams is not allowed.");
    this->BaseStreams.insert(Other);
    Other->DependentStreams.insert(this);
  }

  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addMissingAccess();
    }
    this->Iters++;
    this->TotalIters++;
  }

  virtual bool isAliased() const { return false; }
  virtual std::string formatName() const = 0;

protected:
  std::unordered_set<Stream *> BaseStreams;
  std::unordered_set<Stream *> DependentStreams;
  bool Qualified;
  /**
   * Stores the total iterations for this stream
   */
  size_t TotalIters;
  size_t TotalAccesses;
  size_t TotalStreams;
  /**
   * Maintain the current iteration. Will be reset by endStream() and update by
   * endIter().
   * It should start at 1.
   */
  size_t Iters;
  /**
   * Maintain the iteration when the last addAccess() is called.
   * When endIter(), we check that LastAccessIters == Iters to detect missint
   * access in the last iteration.
   * It should be reset to 0 (should be less than reset value of Iters).
   */
  size_t LastAccessIters;
  MemoryPattern Pattern;
};

class InductionVarStream : public Stream {
public:
  InductionVarStream(
      const llvm::PHINode *_PHIInst, const llvm::Loop *_Loop,
      std::unordered_set<const llvm::Instruction *> &&_ComputeInsts)
      : Stream(TypeT::IV), PHIInst(_PHIInst), Loop(_Loop),
        ComputeInsts(std::move(_ComputeInsts)) {}

  InductionVarStream(const InductionVarStream &Other) = delete;
  InductionVarStream(InductionVarStream &&Other) = delete;
  InductionVarStream &operator=(const InductionVarStream &Other) = delete;
  InductionVarStream &operator=(InductionVarStream &&Other) = delete;

  const llvm::PHINode *getPHIInst() const { return this->PHIInst; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const std::unordered_set<const llvm::Instruction *> &getComputeInsts() const {
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

  static bool isStepInst(const llvm::Instruction *Inst) {
    auto Opcode = Inst->getOpcode();
    switch (Opcode) {
    case llvm::Instruction::Add: {
      return true;
    }
    default: { return false; }
    }
  }

  std::string formatName() const override {
    return "(IV " + LoopUtils::getLoopId(this->Loop) + " " +
           LoopUtils::formatLLVMInst(this->PHIInst) + ")";
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
  const llvm::Loop *Loop;
  std::unordered_set<const llvm::Instruction *> ComputeInsts;
};
#endif