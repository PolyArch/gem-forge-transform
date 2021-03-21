/**
 * Simple memmove.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "immintrin.h"

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, uint64_t *histogram, int N) {
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
  for (int64_t i = 0; i < N; ++i) {
    Value v = a[i];
    histogram[(v >> 24) & 0xFF]++;
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;

int main() {

  const int HistogramSize = 256;
  uint64_t histogram[HistogramSize];
  uint64_t expected[HistogramSize];
#pragma clang loop vectorize(disable)
  for (int64_t i = 0; i < HistogramSize; ++i) {
    expected[i] = 0;
    histogram[i] = 0;
  }

  Value *a = aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (long long i = 0; i < N; i++) {
    a[i] = i << 24;
    expected[i & 0xFF]++;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(histogram, HistogramSize);
#endif

  gf_reset_stats();
  volatile Value c = foo(a, histogram, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < HistogramSize; i++) {
    if (histogram[i] != expected[i]) {
      printf("Hist %d Ret = %lu, Expected = %lu.\n", i, histogram[i],
             expected[i]);
      gf_panic();
    }
  }
  printf("Result correct.\n");
#endif

  return 0;
}
