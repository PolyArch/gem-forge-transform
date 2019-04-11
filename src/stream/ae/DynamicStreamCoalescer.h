#ifndef LLVM_TDG_STREAM_COALESCER_H
#define LLVM_TDG_STREAM_COALESCER_H

#include "FunctionalStream.h"

/**
 * Try to coalescer direct memory streams with the same step root.
 */
class DynamicStreamCoalescer {
 public:
  using CoalescerMatrixT = std::vector<std::vector<uint32_t>>;
  DynamicStreamCoalescer(
      const std::unordered_set<FunctionalStream *> &FuncStreams);

  DynamicStreamCoalescer(const DynamicStreamCoalescer &Other) = delete;
  DynamicStreamCoalescer(DynamicStreamCoalescer &&Other) = delete;
  DynamicStreamCoalescer &operator=(const DynamicStreamCoalescer &Other) =
      delete;
  DynamicStreamCoalescer &operator=(DynamicStreamCoalescer &&Other) = delete;

  ~DynamicStreamCoalescer();

  void updateCoalesceMatrix();

  void finalize();

 private:
  std::unordered_map<FunctionalStream *, size_t> FSIdMap;
  CoalescerMatrixT CoalescerMatrix;
  uint32_t TotalSteps;

  std::vector<int> UFArray;

  std::unordered_map<int, int> LocalToGlobalCoalesceGroupMap;

  void coalesce(int A, int B);
  int findRoot(int X) const;
  int findRoot(int X);

  /**
   * Helper function to determine direct memory stream.
   */
  static bool isDirectMemStream(FunctionalStream *FuncS);
  static int allocateGlobalCoalesceGroup();
  static int AllocatedGlobalCoalesceGroup;
};

#endif