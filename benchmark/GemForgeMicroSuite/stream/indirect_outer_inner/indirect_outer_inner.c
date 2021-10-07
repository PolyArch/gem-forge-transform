#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * This is used to test indirect reuse pattern A[B[i] + C[j]].
 */
typedef int Value;
typedef int64_t IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(Value *a, IndT *ia, IndT *ja, int N,
                                         int M) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    IndT idxI = ia[i];
    for (IndT j = 0; j < M; ++j) {
      IndT idxJ = ja[j];
      sum += a[idxI + idxJ];
    }
  }
  return sum;
}

__attribute__((noinline)) Value foo(Value *a, IndT *ia, IndT *ja, int N,
                                    int M) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    IndT idxI = ia[i];
#pragma clang loop unroll(disable) vectorize(disable)
    for (IndT j = 0; j < M; ++j) {
      IndT idxJ = ja[j];
      sum += a[idxI + idxJ];
    }
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 65536 * 2;
const int M = 32;
Value a[N + M];
IndT ia[N];
IndT ja[M];

int main() {
  // Initialize the index array simply to i.
  for (int i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i;
  }
  for (int i = 0; i < M; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ja[i] = i;
  }

  // Initialize the data array.
  for (int i = 0; i < N + M; ++i) {
    a[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  Value ret = foo_warm(a, ia, ja, N, M);
  gf_reset_stats();
#endif
  Value computed = foo(a, ia, ja, N, M);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * M;
  printf("Computed = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}