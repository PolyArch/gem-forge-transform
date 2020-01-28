/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <stdio.h>

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N, int False) {
  Value sum = 0.0f;
  Value *a = *pa;
#pragma clang loop vectorize(disable) unroll(disable)
  for (uint64_t i = 0; i < N; i += STRIDE) {
    if (False) {
      sum += a[i];
    }
  }
  return sum;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
#endif

  Value *pa = 0;

  m5_reset_stats(0, 0);
  volatile Value c = foo(&pa, N, 0);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  printf("Ret = %f, Expected = %f.\n", c, expected);
#endif

  return 0;
}
