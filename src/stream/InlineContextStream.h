#ifndef LLVM_TDG_INLINE_CONTEXT_STREAM_H
#define LLVM_TDG_INLINE_CONTEXT_STREAM_H

#include "MemoryFootprint.h"
#include "stream/InlineContext.h"
#include "stream/StreamPattern.h"

class InlineContextStream {
public:
  InlineContextStream(const ContextInst &_CInst, const ContextLoop &_CLoop,
                      size_t _LoopLevel)
      : CInst(_CInst), CLoop(_CLoop), LoopLevel(_LoopLevel), TotalIters(0),
        TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
        Pattern() {
    auto Opcode = this->CInst.Inst->getOpcode();
    assert((Opcode == llvm::Instruction::Load ||
            Opcode == llvm::Instruction::Store) &&
           "Should be load/store instruction to build a stream.");
    this->searchAddressComputeInstructions();
  }

  void addAccess(uint64_t Addr) {
    this->Footprint.access(Addr);
    this->LastAccessIters = this->Iters;
    this->Pattern.addAccess(Addr);
    this->TotalAccesses++;
  }
  void addMissingAccess() { this->Pattern.addMissingAccess(); }
  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->addMissingAccess();
    }
    this->Iters++;
    this->TotalIters++;
  }
  void endStream() {
    this->Pattern.endStream();
    this->Iters = 1;
    this->LastAccessIters = 0;
    this->TotalStreams++;
  }
  void finalizePattern() { this->Pattern.finalizePattern(); }

  const ContextLoop &getContextLoop() const { return this->CLoop; }
  const ContextInst &getContextInst() const { return this->CInst; }
  size_t getLoopLevel() const { return this->LoopLevel; }
  const MemoryFootprint &getFootprint() const { return this->Footprint; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  const StreamPattern &getPattern() const { return this->Pattern; }

  bool isIndirect() const { return !this->BaseLoads.empty(); }
  size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
  size_t getNumAddrInsts() const { return this->AddrInsts.size(); }
  const std::unordered_set<ContextInst> &getBaseLoads() const {
    return this->BaseLoads;
  }

private:
  const ContextInst CInst;
  ContextLoop CLoop;
  const size_t LoopLevel;
  MemoryFootprint Footprint;
  std::unordered_set<ContextInst> BaseLoads;
  std::unordered_set<ContextInst> AddrInsts;

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
  StreamPattern Pattern;

  void searchAddressComputeInstructions();

  /**
   * Helper function to look up into the context to resolve argument to
   * instruction.
   */
  static llvm::Instruction *resolveArgument(InlineContextPtr &Context,
                                            llvm::Argument *Arg);
};

#endif