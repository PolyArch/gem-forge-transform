/**
 * Simple read after write for memory accesses.
 */
#include "../../gfm_utils.h"
#include <stdio.h>

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(volatile Value *a, int N) {
// Make sure there is no reuse.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 1; i < N; i += 1) {
    Value prev = a[i - 1];
    a[i] += prev;
  }
  return a[N - 1];
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];

int main() {

  gf_detail_sim_start();
#ifdef WARM_CACHE
  for (int i = 0; i < N; ++i) {
    a[i] = 1;
  }
#endif
  gf_reset_stats();
  volatile Value computed = foo(a, N);
  gf_detail_sim_end();

#ifdef CHECK
  printf("Computed = %d, Expected = %d.\n", computed, N);
  if (computed != N) {
    // gf_panic();
  }
#endif

  return 0;
}
