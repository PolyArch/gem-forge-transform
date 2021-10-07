/**
 * Simple coalesced sum copy.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int N, Value offset) {
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 1; i < N; i += STRIDE) {
    Value va = a[i];
    Value vap = a[i - 1];
    b[i] = va + vap + offset;
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];
Value b[N];

int main() {

 gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = 1;
  }
  gf_reset_stats();
#endif

  const Value offset = 1;
  volatile Value c = foo(a, b, N, offset);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 1; i < N; i += STRIDE) {
    expected += a[i] + a[i - 1] + offset;
    computed += b[i];
  }
  printf("Ret = %lld, Expected = %lld.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
