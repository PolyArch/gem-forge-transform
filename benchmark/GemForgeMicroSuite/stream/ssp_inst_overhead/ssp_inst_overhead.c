#include "../../gfm_utils.h"
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

__attribute__((noinline)) Value foo_warm(Value *a, Value *b, IndT *ia, IndT *ja,
                                         int N, int M) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    IndT idxI = ia[i];
    for (IndT j = 0; j < M; ++j) {
      IndT idxJ = ja[j];
      Value va = a[idxI + idxJ];
      Value vb = b[idxI + idxJ];
      Value tmp = va - vb;
      sum += tmp * tmp;
    }
  }
  return sum;
}

__attribute__((noinline)) Value foo(Value *a, Value *b, IndT *ia, IndT *ja,
                                    int N, int M) {
  Value sum = 0.0;
#pragma clang loop unroll(disable) vectorize(disable)
  for (IndT i = 0; i < N; i += STRIDE) {
    IndT idxI = ia[i];
#pragma clang loop unroll(disable) vectorize(disable)
    for (IndT j = 0; j < M; ++j) {
      IndT idxJ = ja[j];
      Value va = a[idxI + idxJ];
      Value vb = b[idxI + idxJ];
      Value tmp = va - vb;
      sum += tmp * tmp;
    }
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 7;
const int M = 14;
Value a[N + M];
Value b[N + M];
IndT ia[N];
IndT ja[M];

int main() {
  // Initialize the index array simply to 0.
  for (int i = 0; i < N; ++i) {
    ia[i] = 0;
  }
  for (int i = 0; i < M; ++i) {
    ja[i] = i;
  }

  // Initialize the data array.
  for (int i = 0; i < N + M; ++i) {
    a[i] = 1;
    b[i] = 0;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  Value ret = foo_warm(a, b, ia, ja, N, M);
  gf_reset_stats();
#endif
  // We compute for 10000 times.
  Value computed = 0;
  for (int i = 0; i < 10000; ++i) {
    volatile Value v = foo(a, b, ia, ja, N, M);
    computed += v;
  }
  computed /= 10000;
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