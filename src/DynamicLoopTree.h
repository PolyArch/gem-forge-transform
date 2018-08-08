#ifndef LLVM_TDG_DYNAMIC_LOOP_TREE_H
#define LLVM_TDG_DYNAMIC_LOOP_TREE_H

#include "DataGraph.h"
#include "LoopUtils.h"

#include <unordered_map>

/**
 * This class represent one iteration of a loop.
 * It contains the pointer to next iteration, as well as all nested loop
 * iteration.
 */
class DynamicLoopIteration {
public:
  using DynamicInstIter = DataGraph::DynamicInstIter;
  DynamicLoopIteration(llvm::Loop *_Loop, DynamicInstIter _End);
  ~DynamicLoopIteration();

  DynamicLoopIteration(const DynamicLoopIteration &Other) = delete;
  DynamicLoopIteration(DynamicLoopIteration &&Other) = delete;
  DynamicLoopIteration &operator=(const DynamicLoopIteration &Other) = delete;
  DynamicLoopIteration &operator=(DynamicLoopIteration &&Other) = delete;

  void addInst(DynamicInstIter InstIter);

  void addInstEnd(DynamicInstIter InstIter);

  /**
   * Count the number of iterations so far.
   */
  size_t countIter() const;

  /**
   * Get the tail iteration, and move the ownership to the caller.
   */
  DynamicLoopIteration *moveTailIter();

  DynamicLoopIteration *getNextIter() const { return this->NextIter; }

  DynamicLoopIteration *getChildIter(llvm::Loop *Child);

  llvm::Loop *getLoop() const { return this->Loop; }

  DynamicInstIter begin() { return this->Start; }
  DynamicInstIter end() { return this->End; }

  DynamicInstruction *getDynamicInst(llvm::Instruction *StaticInst) const;

private:
  llvm::Loop *Loop;
  // Iterator pointing to the start and end of this iteration.
  DynamicInstIter Start;
  DynamicInstIter End;
  // Contains map to the iteration of nested loops.
  std::unordered_map<llvm::Loop *, DynamicLoopIteration *> NestLoopIters;
  // Pointer to the next iteration.
  DynamicLoopIteration *NextIter;

  // Map from static instruction to the dynamic inst.
  std::unordered_map<llvm::Instruction *, DynamicInstruction *>
      StaticToDynamicMap;
  enum {
    EMPTY,
    BUFFERING,
    COMPLETED,
  } Status;
};

#endif