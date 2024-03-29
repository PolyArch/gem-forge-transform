#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

// Simple indirect access.
typedef int Value;
typedef int64_t IndT;

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
  for (IndT i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 65536 * 2;
const int NN = 65536 * 16;
Value a[NN];
IndT ia[N];

int main() {
  // Initialize the index array.
  for (int i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i * (NN / N);
  }

  // Initialize the data array.
  for (int i = 0; i < NN; ++i) {
    a[i] = i;
  }

  // Shuffle it.
  for (int j = N - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    IndT tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  Value ret = foo_warm(a, ia, N);
  printf("Warmup cache, A[] Vaddr %p.\n", a);
  gf_reset_stats();
#endif
  Value computed = foo(a, ia, N);
  gf_detail_sim_end();

#ifdef CHECK
#ifdef WARM_CACHE
  Value expected = ret;
#else
  Value expected = foo_warm(a, ia, N);
#endif
  printf("Computed = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}