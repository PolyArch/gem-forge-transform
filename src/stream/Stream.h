#ifndef LLVM_TDG_STREAM_STREAM_H
#define LLVM_TDG_STREAM_STREAM_H

#include "LoopUtils.h"
#include "MemoryPattern.h"
#include "Utils.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"

#include <fstream>
#include <string>
#include <unordered_set>

class Stream {
public:
  enum TypeT {
    IV,
    MEM,
  };
  const TypeT Type;
  Stream(TypeT _Type, const std::string &_Folder,
         const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
         size_t _LoopLevel);
  ~Stream();

  bool hasNoBaseStream() const { return this->BaseStreams.empty(); }
  const std::unordered_set<Stream *> &getBaseStreams() const {
    return this->BaseStreams;
  }
  const std::unordered_set<Stream *> &getDependentStreams() const {
    return this->DependentStreams;
  }
  void markQualified() { this->Qualified = true; }
  bool isQualified() const { return this->Qualified; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const llvm::Instruction *getInst() const { return this->Inst; }
  size_t getLoopLevel() const { return this->LoopLevel; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  const MemoryPattern &getPattern() const { return this->Pattern; }

  void addBaseStream(Stream *Other) {
    // assert(Other != this && "Self dependent streams is not allowed.");
    this->BaseStreams.insert(Other);
    Other->DependentStreams.insert(this);
  }

  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addMissingAccess();
      this->LastAccessIters = this->Iters;
    }
    this->Iters++;
    this->TotalIters++;
  }

  virtual bool isAliased() const { return false; }
  std::string formatType() const {
    switch (this->Type) {
    case IV:
      return "IV";
    case MEM:
      return "MEM";
    default: { llvm_unreachable("Illegal stream type to be formatted."); }
    }
  }
  std::string formatName() const {
    return "(" + this->formatType() + " " + LoopUtils::getLoopId(this->Loop) +
           " " + LoopUtils::formatLLVMInst(this->Inst) + ")";
  }
  virtual const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const = 0;

  static bool isStepInst(const llvm::Instruction *Inst) {
    auto Opcode = Inst->getOpcode();
    switch (Opcode) {
    case llvm::Instruction::Add: {
      return true;
    }
    default: { return false; }
    }
  }

protected:
  /**
   * Stores the information of the stream.
   */
  std::string Folder;
  std::string FullPath;
  std::ofstream StoreFStream;

  /**
   * Stores the instruction and loop.
   */
  const llvm::Instruction *Inst;
  const llvm::Loop *Loop;
  const size_t LoopLevel;

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

#endif