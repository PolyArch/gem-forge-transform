/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int N, Value offset) {
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 1; i < N; i += STRIDE) {
    b[i] = a[i] + a[i - 1] + offset;
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];
Value b[N];

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = 1;
  }
  m5_reset_stats(0, 0);
#endif

  const Value offset = 1.0f;
  volatile Value c = foo(a, b, N, offset);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 1; i < N; i += STRIDE) {
    expected += a[i] + a[i - 1] + offset;
    computed += b[i];
  }
  printf("Ret = %f, Expected = %f.\n", computed, expected);
#endif

  return 0;
}
