
#ifndef LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PRECOMPUTATION_SLICE_H
#define LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PRECOMPUTATION_SLICE_H

#include "DataGraph.h"

#include "llvm/IR/Instructions.h"

#include <list>

class PrecomputationSlice {
 public:
  PrecomputationSlice(llvm::LoadInst *_CriticalInst, DataGraph *_DG);

  PrecomputationSlice(const PrecomputationSlice &Other) = delete;
  PrecomputationSlice &operator=(const PrecomputationSlice &Other) = delete;

  PrecomputationSlice(PrecomputationSlice &&Other) = delete;
  PrecomputationSlice &operator=(PrecomputationSlice &&Other) = delete;

 private:
  llvm::LoadInst *CriticalInst;
  DataGraph *DG;
  std::list<llvm::Instruction *> StaticSlice;
  std::list<DynamicInstruction *> DynamicSlice;

  void initializeSlice();
};

#endif