/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  // Make sure there is no reuse.
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    if (a[i] & 1) {
      b[i] = 0;
      c[i] = 1;
    }
  }
  return 0;
}

// 65536*4 is 512kB.
const long long N = 65536;
Value a[N];
Value b[N];
Value c[N];

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
  long long expectedB = N * (N - 2) / 2;
  // long long expectedC = N * N / 4;
  long long expectedC = N / 2;
  long long computedB = 0;
  long long computedC = 0;
  printf("Start computed.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    computedB += b[i];
    computedC += c[i];
  }
  printf("ExpectedB = %lld ComputedB = %lld.\n", expectedB, computedB);
  printf("ExpectedC = %lld ComputedC = %lld.\n", expectedC, computedC);
  // for (int i = 0; i < N; i += STRIDE) {
  //   printf("i = %d a = %f b = %f c = %f c_x = %f.\n",
  //     i, a[i], b[i], c[i]);
  // }
#endif

  return 0;
}
