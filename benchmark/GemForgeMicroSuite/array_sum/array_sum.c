/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  Value *a = *pa;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[i];
  }
  return sum;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
#endif

  Value *pa = a;
  m5_detail_sim_start();
  volatile Value c = foo(&pa, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  printf("Ret = %llu, Expected = %llu.\n", c, expected);
#endif

  return 0;
}
