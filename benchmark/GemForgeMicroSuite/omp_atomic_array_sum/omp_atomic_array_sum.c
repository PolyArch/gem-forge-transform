
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>

/**
 * Used for random test purpose.
 */

typedef long long Value;

// #define STRIDE (CACHE_LINE_SIZE / sizeof(Value))
#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N) {
  Value ret = 0;
#pragma omp parallel firstprivate(a, N)
  {
    Value sum = 0;
#pragma nounroll
    for (long long i = 0; i < N; i += STRIDE) {
      sum += __atomic_fetch_add(a + i, 1, __ATOMIC_RELAXED);
    }
    __atomic_fetch_add(&ret, sum, __ATOMIC_RELAXED);
  }
  return ret;
}

// 65536*8 is 512kB.
const int N = 65536;
// const int N = 128;
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

  // Initialize the array.
  for (long long i = 0; i < N; i++) {
    a[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * numThreads * (numThreads + 1) / 2;
  int allMatch = 1;
  for (int i = 0; i < N; i += STRIDE) {
    Value v = a[i];
    Value expectedAi = 1 + numThreads;
    if (v != expectedAi) {
      printf("Mismatch at A[%d], expected %lld, got %lld.\n", i, expectedAi, v);
      allMatch = 0;
    }
  }
  printf("Computed %lld. Expected %lld. AllMatch %d.\n", computed, expected,
         allMatch);
  if (!allMatch || expected != computed) {
    gf_panic();
  }
#endif

  return 0;
}