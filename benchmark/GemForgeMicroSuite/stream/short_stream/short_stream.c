/**
 * Short stream stress test for stream configuration overhead.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N, int M) {
  Value sum = 0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (int j = 0; j < M; ++j) {
#pragma clang loop vectorize(disable) unroll(disable)
    for (int i = 0; i < N; i += STRIDE) {
      sum = sum + a[i + j * N];
    }
  }
  return sum;
}

// 65536*8 is 512kB.
// const int N = 65536 * 2;
const int N = 5;
const int M = 1024;
Value a[N * M];

int main() {

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N * M; i++) {
    a[i] = i % N;
  }
  gf_reset_stats();
#endif

  volatile Value computed = foo(a, N, M);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  expected = expected * M;
  printf("Computed = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
