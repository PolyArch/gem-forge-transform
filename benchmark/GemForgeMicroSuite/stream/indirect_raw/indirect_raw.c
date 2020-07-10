#include "../../gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * This is used to test aliasing detection in the implementation.
 * Indirect stores to B[i] aliased with direct load to B[i - 1].
 */
typedef int Value;
typedef int IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
  for (IndT i = 1; i < N; i += STRIDE) {
    sum += a[i - 1];
    a[ia[i]] = 2;
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 65536 * 2;
Value a[N];
IndT ia[N];

int main() {
  // Initialize the index array simply to i.
  for (int i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i;
  }

  // Initialize the data array.
  for (int i = 0; i < N; ++i) {
    a[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  Value ret = foo_warm(a, ia, N);
  gf_reset_stats();
#endif
  Value computed = foo(a, ia, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 2 * (N - 1) - 1;
  printf("Computed = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}