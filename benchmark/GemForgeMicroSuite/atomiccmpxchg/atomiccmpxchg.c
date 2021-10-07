
#include <math.h>
#include <stdio.h>

#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

typedef int Value;

/**
 * Used for random test purpose.
 */

// 65536*8 is 512kB.
const int N = 1024;
const int STEP_AHEAD = 16;
Value a[N + STEP_AHEAD * 16];
#define STRIDE 16

#define SWAP_VALUE 4

__attribute__((noinline)) Value foo(Value **pa, int N) {
  volatile Value *a = *pa;
  Value sum = 0;
#pragma nounroll
  for (long long i = 0; i < N; i += STRIDE) {
    Value cmp = 2;
    if (__atomic_compare_exchange_n(a + i, &cmp, SWAP_VALUE, 0 /* weak */,
                                    __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
      sum++;
    }
  }
  return sum;
}

int main() {
  volatile Value c;
  Value *pa = a;
  // This should warm up the cache.
  for (long long i = 0; i < N; i += STRIDE) {
    if (((i / STRIDE) % 4) == 0) {
      a[i] = 2;
    }
  }
#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif
  c = foo(&pa, N);
#ifdef GEM_FORGE
  m5_detail_sim_end();
#endif
  for (int i = 0; i < N; i += STRIDE) {
    Value expected = 0;
    if (((i / STRIDE) % 4) == 0) {
      expected = SWAP_VALUE;
    }
    if (a[i] != expected) {
      printf("Mismatch at a[%d] = %d, expected %d.\n", i, a[i], expected);
    }
  }
  printf("Ret = %d, Expected %d.\n", c, (Value)((N / STRIDE / 4)));
  return 0;
}
