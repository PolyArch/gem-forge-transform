#ifndef LLVM_TDG_STREAM_COALESCER_H
#define LLVM_TDG_STREAM_COALESCER_H

#include "FunctionalStream.h"

/**
 * Try to coalescer direct memory streams with the same step root.
 */
class DynamicStreamCoalescer {
public:
  using CoalescerMatrixT = std::vector<std::vector<uint32_t>>;
  DynamicStreamCoalescer(FunctionalStream *_RootStream);

  DynamicStreamCoalescer(const DynamicStreamCoalescer &Other) = delete;
  DynamicStreamCoalescer(DynamicStreamCoalescer &&Other) = delete;
  DynamicStreamCoalescer &operator=(const DynamicStreamCoalescer &Other) = delete;
  DynamicStreamCoalescer &operator=(DynamicStreamCoalescer &&Other) = delete;

  ~DynamicStreamCoalescer();

  void updateCoalesceMatrix();

  void finalize();

  int getCoalesceGroup(FunctionalStream *FS) const;

private:
  FunctionalStream *RootStream;
  std::unordered_map<FunctionalStream *, size_t> FSIdMap;
  CoalescerMatrixT CoalescerMatrix;
  uint32_t TotalSteps;

  std::vector<int> UFArray;

  void coalesce(int A, int B);
  int findRoot(int X) const;
  int findRoot(int X);

  /**
   * Helper function to determine direct memory stream.
   */
  static bool isDirectMemStream(FunctionalStream *FuncS);
};

#endif