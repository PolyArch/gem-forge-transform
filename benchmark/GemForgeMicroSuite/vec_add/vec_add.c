/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  // Make sure there is no reuse.
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
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

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i;
    c[i] = i;
  }
#endif

  m5_detail_sim_start();
  volatile Value ret = foo(a, b, c, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i] + b[i];
    computed += c[i];
  }
  printf("Ret = %llu, Expected = %llu.\n", computed, expected);
#endif

  return 0;
}
