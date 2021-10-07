/**
 * Simple array sum.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N, int M) {
  Value sum = 0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    sum = sum + a[i];
  }
  return sum;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
// const int N = 128;
Value a[N];

int main() {

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  gf_reset_stats();
#endif

  volatile Value computed = foo(a, N, 10);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  printf("Computed = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
