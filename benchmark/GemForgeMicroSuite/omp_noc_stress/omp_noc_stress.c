/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <omp.h>
#include <stdio.h>

typedef int Value;

#define STRIDE 16
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  Value *a = *pa;
// #pragma clang loop vectorize(disable)
// #pragma clang loop unroll(disable)
#pragma omp parallel for firstprivate(N, a) reduction(+ : sum) schedule(static)
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[i];
  }
  return sum;
}

// 32MB data.
const int N = 32 * 1024 * 1024 / sizeof(Value);
Value a[N];

#define NUM_THREADS 16

int main() {

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
#endif

  omp_set_dynamic(0);
  omp_set_num_threads(NUM_THREADS);
  omp_set_schedule(omp_sched_static, 0);

  Value *pa = a;
  m5_reset_stats(0, 0);
  volatile Value c = foo(&pa, N);
  m5_detail_sim_end();
  exit(0);

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
