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

__attribute__((noinline)) Value foo(Value *a, Value *b, int N) {
  __m512 valS = _mm512_set1_ps(0.0f);
  for (int i = 0; i < N; i += 16) {
    __m512 valA = _mm512_load_ps(a + i);
    __m512 valB = _mm512_load_ps(b + i);
    __m512 valM = _mm512_mul_ps(valA, valB);
    valS = _mm512_add_ps(valM, valS);
  }
  Value sum = _mm512_reduce_add_ps(valS);
  return sum;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
// const int N = 1024;
Value a[N];
Value b[N];

int main() {

  gf_detail_sim_start();
#ifdef WARM_CACHE
// This should warm up the cache.
// We avoid AVX since we only have partial AVX support.
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i % 3;
  }
  gf_reset_stats();
#endif

  volatile Value computed = foo(a, b, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i] * b[i];
  }
  printf("Computed = %f, Expected = %f.\n", computed, expected);
  if ((fabs(computed - expected) / expected) > 0.01f) {
    gf_panic();
  }
#endif

  return 0;
}
