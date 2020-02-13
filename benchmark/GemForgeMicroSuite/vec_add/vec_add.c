/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  // Make sure there is no reuse.
#pragma clang loop vectorize(enable)
// #pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    c[i] = a[i] + b[i];
  }
  return c[0];
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];
Value b[N];
Value c[N];
Value c_x[N];

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
    c[i] = 0;
  }
#endif
  m5_reset_stats(0, 0);
  volatile Value ret = foo(a, b, c, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  printf("Start c_x.\n");
  #pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    c_x[i] = a[i] + b[i];
  }
  printf("Start expected.\n");
  #pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    expected += c_x[i];
  }
  printf("Start computed.\n");
  #pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    computed += c[i];
  }
  printf("Ret = %f, Expected = %f Real = %d.\n", computed, expected, N * (N - 1) * 3 / 2);
  // for (int i = 0; i < N; i += STRIDE) {
  //   printf("i = %d a = %f b = %f c = %f c_x = %f.\n",
  //     i, a[i], b[i], c[i], c_x[i]);
  // }
#endif

  return 0;
}
