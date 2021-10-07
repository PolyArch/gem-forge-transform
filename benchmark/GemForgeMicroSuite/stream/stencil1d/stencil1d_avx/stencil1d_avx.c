#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define STRIDE 1
#define STORE_OFFSET 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  for (int i = 0; i < N; i += 16) {
    __m512 valA0 = _mm512_load_ps(a + i);
    __m512 valA1 = _mm512_load_ps(a + i + 1);
    __m512 valA2 = _mm512_load_ps(a + i + 2);
    __m512 valA = _mm512_sub_ps(_mm512_add_ps(valA0, valA2), valA1);
    __m512 valB = _mm512_load_ps(b + i);
    __m512 valM = _mm512_add_ps(valA, valB);
    _mm512_store_ps(c + i + STORE_OFFSET, valM);
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
int main() {

  Value *a = (Value *)aligned_alloc(CACHE_LINE_SIZE, (N + 2) * sizeof(Value));
  Value *b = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  Value *c = (Value *)aligned_alloc(CACHE_LINE_SIZE,
                                    (N + STORE_OFFSET) * sizeof(Value));

#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N + 2; i++) {
    a[i] = i;
  }
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    b[i] = i % 3;
  }
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N + STORE_OFFSET; i++) {
    c[i] = 0;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N + 2);
  WARM_UP_ARRAY(b, N);
  WARM_UP_ARRAY(c, N);
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < N; i += STRIDE) {
    Value expected = a[i] + a[i + 2] - a[i + 1] + b[i];
    Value computed = c[i + STORE_OFFSET];
    if ((fabs(computed - expected) / expected) > 0.01f) {
      printf(
          "Error at %d, A %f - %f + %f + B %f, Computed = %f, Expected = %f.\n",
          i, a[i], a[i + 1], a[i + 2], b[i], computed, expected);
      gf_panic();
    }
  }
  printf("All results match.\n");
#endif

  return 0;
}
