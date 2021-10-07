/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  for (int i = 0; i < N; i += 16) {
    __m512 valA = _mm512_load_ps(a + i);
    __m512 valB = _mm512_load_ps(b + i);
    __m512 valM = _mm512_add_ps(valA, valB);
    _mm512_store_ps(c + i, valM);
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
// const int N = 1024;
Value a[N];
Value b[N];
Value c[N];

int main() {

#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    b[i] = i % 3;
  }
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    c[i] = 0;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(b, N);
  WARM_UP_ARRAY(c, N);
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < N; i += STRIDE) {
    Value expected = a[i] + b[i];
    Value computed = c[i];
    if ((fabs(computed - expected) / expected) > 0.01f) {
      printf("Error at %d, A %f B %f C %f, Computed = %f, Expected = %f.\n", i,
             a[i], b[i], c[i], computed, expected);
      gf_panic();
    }
  }
  printf("All results match.\n");
#endif

  return 0;
}
