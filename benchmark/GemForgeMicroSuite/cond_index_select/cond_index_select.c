/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N, int w) {
  Value ret = 0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 1; i < N; i += STRIDE) {
    Value curr = a[i];
    int cond = ret == 0 && curr >= w;
    ret = cond ? i : ret;
  }
  return ret;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  m5_reset_stats(0, 0);
#endif

  volatile Value c = foo(a, N, N / 2);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    Value prev = a[i - 1];
    Value curr = a[i];
    Value w = N / 2;
    if (prev < w && curr >= w) {
      expected = i;
    }
  }
  printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
