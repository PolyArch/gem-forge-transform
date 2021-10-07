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

__attribute__((noinline)) Value foo(Value *a, int N) {
  Value sum = 0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    Value v = a[i];
    if ((v & 0x1) == 0) {
      sum += v;
    }
  }
  return sum;
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

  volatile Value c = foo(a, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    Value v = a[i];
    if ((v & 0x1) == 0) {
      expected += v;
    }
  }
  printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
