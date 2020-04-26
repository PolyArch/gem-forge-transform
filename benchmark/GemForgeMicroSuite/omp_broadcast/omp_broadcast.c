/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <malloc.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N, int numThreads) {
  Value sum = 0.0f;
  Value *a = *pa;
// #pragma clang loop vectorize(disable)
// #pragma clang loop unroll(disable)
#pragma omp parallel for firstprivate(N, a) reduction(+ : sum) schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
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

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
  m5_reset_stats(0, 0);
#endif

  Value *pa = a;
  volatile Value c = foo(&pa, N, numThreads);
  m5_detail_sim_end();
  exit(0);

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += a[i];
  }
  expected *= numThreads;
  printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
