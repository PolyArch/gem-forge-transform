/**
 * Simple dense vector dot product.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, Value **pb, int N) {
  Value c = 0.0f;
  Value *a = *pa;
  Value *b = *pb;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    c += a[i] * b[i];
  }
  return c;
}

// 65536*4 is 512kB.
const int N = 65536 / 2;
Value a[N];
Value b[N];

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i;
  }
#endif

  Value *pa = a;
  Value *pb = b;
  m5_detail_sim_start();
  volatile Value c = foo(&pa, &pb, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i] * b[i];
  }
  printf("Ret = %llu, Expected = %llu.\n", c, expected);
#endif

  return 0;
}
