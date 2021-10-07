#include "gfm_utils.h"
#include "../../ssp.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * This is used to test flatten load stream like:
 * int k = 0
 * for i = 0 to N
 *   for j = 0 to M
 *     a[k]
 *     k++
 *
 * Currently I just want to try with the new manual interface.
 */
typedef int64_t Value;
typedef int64_t IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N, int M) {
  Value sum = 0.0;
  const StreamIdT s = ssp_config(sizeof(a[0]), a);
#pragma clang loop unroll(disable) vectorize(disable)
  for (IndT i = 0; i < N; i += STRIDE) {
#pragma clang loop unroll(disable) vectorize(disable)
    for (IndT j = 0; j < M; ++j) {
      sum += ssp_load_i32(s);
      ssp_step(s, 1);
    }
  }
  ssp_end(s);
  return sum;
}

__attribute__((noinline)) Value foo_warm(Value *a, int N, int M) {
  Value sum = 0.0;
  IndT k = 0;
  for (IndT i = 0; i < N; i += STRIDE) {
    for (IndT j = 0; j < M; ++j) {
      sum += a[k];
      k++;
    }
  }
  return sum;
}

// 65536 * 2 * 8 = 1MB
const Value N = 65536;
const Value M = 2;
Value a[N * M];

int main() {
  // Initialize the data array.
  for (int i = 0; i < N * M; ++i) {
    a[i] = i;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  Value ret = foo_warm(a, N, M);
  gf_reset_stats();
#endif
  Value computed = foo(a, N, M);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * M * (N * M - 1) / 2;
  printf("Computed = %ld, Expected = %ld.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}