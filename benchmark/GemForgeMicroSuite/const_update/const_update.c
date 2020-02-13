/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N) {
  Value sum = 0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[i];
    a[i] = 2;
  }
  return sum;
}

// 65536*8 = 512kB.
const int N = 65536;
Value a[N];

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
#endif
  m5_reset_stats(0, 0);
  volatile Value ret = foo(a, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  printf("Start expected.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    expected += 2;
  }
  printf("Start computed.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    computed += a[i];
  }
  printf("Ret = %d, Computed = %d, Expected = %d.\n", ret, computed, expected);
#endif

  return 0;
}
