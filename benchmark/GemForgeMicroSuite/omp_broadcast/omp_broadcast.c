/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

#define NUM_THREADS 16

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  Value *a = *pa;
// #pragma clang loop vectorize(disable)
// #pragma clang loop unroll(disable)
#pragma omp parallel for firstprivate(N, a) reduction(+ : sum) schedule(static)
  for (int tid = 0; tid < NUM_THREADS; ++tid) {
    int tmpSum = 0;
    for (int i = 0; i < N; i += STRIDE) {
      tmpSum += a[i];
    }
    sum += tmpSum;
  }
  return sum;
}

// 1MB data.
const int N = 1 * 1024 * 1024 / sizeof(Value);
Value a[N];

int main() {

  mallopt(M_ARENA_MAX, 2);
  omp_set_dynamic(0);
  omp_set_num_threads(NUM_THREADS);
  omp_set_schedule(omp_sched_static, 0);
  kmp_set_stacksize_s(8*1024*1024);

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < NUM_THREADS; ++tid) {
    printf("Start thread %d with stacksize %zu.\n", tid, kmp_get_stacksize_s());
  }
  m5_reset_stats(0, 0);
#endif

  Value *pa = a;
  volatile Value c = foo(&pa, N);
  m5_detail_sim_end();
  exit(0);

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  expected *= NUM_THREADS;
  printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
