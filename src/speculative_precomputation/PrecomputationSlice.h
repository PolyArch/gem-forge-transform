
#ifndef LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PRECOMPUTATION_SLICE_H
#define LLVM_TDG_SPECULATIVE_PRECOMPUTATION_PRECOMPUTATION_SLICE_H

#include "DataGraph.h"
#include "TDGSerializer.h"

#include "llvm/IR/Instructions.h"

#include <list>

class PrecomputationSlice {
 public:
  PrecomputationSlice(llvm::LoadInst *_CriticalInst, DataGraph *_DG);
  ~PrecomputationSlice();

  PrecomputationSlice(const PrecomputationSlice &Other) = delete;
  PrecomputationSlice &operator=(const PrecomputationSlice &Other) = delete;

  PrecomputationSlice(PrecomputationSlice &&Other) = delete;
  PrecomputationSlice &operator=(PrecomputationSlice &&Other) = delete;

  /**
   * Check if this slice is the same as the other one.
   * So far we just check if the static slice is the same.
   */
  bool isSame(const PrecomputationSlice &Other) const;

  /**
   * Layout the real slice from DynamicSlice.
   */
  void generateSlice(TDGSerializer *Serializer, bool IsReal) const;

 private:
  llvm::LoadInst *CriticalInst;
  DataGraph *DG;

  struct SliceInst {
    LLVMDynamicInstruction *DynamicInst;
    std::unordered_set<SliceInst *> BaseSliceInsts;
    SliceInst(LLVMDynamicInstruction *_DynamicInst)
        : DynamicInst(_DynamicInst) {}
    ~SliceInst() {
      delete this->DynamicInst;
      this->DynamicInst = nullptr;
    }
  };

  std::list<SliceInst *> SliceTemplate;

  void initializeSlice();

  static LLVMDynamicInstruction *copyDynamicLLVMInst(
      DynamicInstruction *DynamicInst);
};

#endif