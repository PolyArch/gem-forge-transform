
#include <math.h>
#include <stdio.h>

#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

typedef long long Value;

/**
 * Used for random test purpose.
 */

// 65536*8 is 512kB.
const int N = 128;
const int STEP_AHEAD = 16;
Value a[N + STEP_AHEAD * 16];
#define STRIDE 8

__attribute__((noinline)) Value foo(Value **pa, int N) {
  volatile Value *a = *pa;
  Value sum = 0;
#pragma nounroll
  for (long long i = 0; i < N; i += STRIDE) {
    a[20] = 1;
    sum += a[i];
  }
  return sum;
}

int main() {
  volatile Value c;
  Value *pa = a;
  // This should warm up the cache.
  for (long long i = 0; i < N; i += STRIDE) {
    a[i] = 1;
  }
#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif
  c = foo(&pa, N);
#ifdef GEM_FORGE
  m5_detail_sim_end();
#endif
  Value sum = 0.0;
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[i];
  }
  printf("Ret = %lld, Expected %lld.\n", sum, (Value)((N + N) / STRIDE));
  return 0;
}
