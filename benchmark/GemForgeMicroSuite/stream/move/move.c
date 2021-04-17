/**
 * Simple memmove.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "immintrin.h"

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int N) {
  memmove(b, a, N * sizeof(Value));
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
const int offset = 2;
Value a[N + offset];
Value *b = a + offset;

int main() {
  for (long long i = 0; i < N + offset; i++) {
    a[i] = i;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N + offset);
#endif

  gf_reset_stats();
  volatile Value c = foo(a, b, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 0; i < N; i++) {
    expected += i;
    computed += b[i];
  }
  printf("Ret = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
