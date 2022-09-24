/**
 * Simple coalesced sum copy.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int N) {
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += 16) {
    ValueAVX val = ValueAVXLoad(a + i);
    ValueAVXStore(b + i, val);
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];
Value b[N];

int main() {
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  for (long long i = 0; i < N; i++) {
    b[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(b, N);
#endif

  gf_reset_stats();
  volatile Value c = foo(a, b, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 0; i < N; i++) {
    expected += a[i];
    computed += b[i];
  }
  printf("Ret = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
