/**
 * Simple dense vector add.
 */

#include "../../gfm_utils.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N) {
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    volatile Value v = a[i];
    a[i] = 2;
  }
  return 0;
}

// 65536*8 = 512kB.
const int N = 65536;
Value a[N];

int main() {

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = 1;
  }
#endif
  gf_reset_stats();
  volatile Value ret = foo(a, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * 2;
  Value computed = 0;
  printf("Start computed.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    computed += a[i];
  }
  printf("Computed = %llu, Expected = %llu.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
