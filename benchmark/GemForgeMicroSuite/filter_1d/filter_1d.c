/**
 * Simple 1d filter.
 * Mainly to test coalesced streams.
 */
#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N, Value **pb) {
  Value sum = 0.0f;
  Value *a = *pa;
  Value *b = *pb;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    Value x = a[i];
    if (i > 0) {
      x += a[i - 1];
    }
    if (i + 1 < N) {
      x += a[i + 1];
    }
    b[i] = x;
  }
  return sum;
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
    b[i] = i;
  }
#endif

  Value *pa = a;
  Value *pb = b;
  m5_reset_stats(0, 0);
  volatile Value c = foo(&pa, N, &pb);
  m5_detail_sim_end();

#ifdef CHECK
  uint64_t mismatch = 0;
  for (int i = 0; i < N; i += STRIDE) {
    Value expected = a[i];
    if (i > 0) {
      expected += a[i - 1];
    }
    if (i + 1 < N) {
      expected += a[i + 1];
    }
    if (expected != b[i]) {
      mismatch++;
    }
  }
  printf("Found mismatch %llu.\n", mismatch);
#endif

  return 0;
}
