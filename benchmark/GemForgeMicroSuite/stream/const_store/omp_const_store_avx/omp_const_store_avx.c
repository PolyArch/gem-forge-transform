/**
 * Simple dense vector add.
 */

#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>

#include "immintrin.h"

typedef ValueT Value;

#define STRIDE 1
// #define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, int N) {
#pragma omp parallel firstprivate(a)
  {
    ValueAVX valS = ValueAVXSet1(2.0f);
#pragma omp for schedule(static)
    for (int i = 0; i < N; i += 16) {
      ValueAVXStore(a + i, valS);
    }
  }
  return 0;
}

// 65536*2*4 = 512kB.
const int N = 1024 * 1024 * 16 / sizeof(Value);

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  Value *a = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
#pragma clang loop vectorize(disable)
  for (long long i = 0; i < N; i++) {
    a[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  WARM_UP_ARRAY(a, N);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif
  gf_reset_stats();
  volatile Value ret = foo(a, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * 2;
  Value computed = 0;
  printf("Start computed.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    Value v = a[i];
    computed += v;
  }
  printf("Computed = %f, Expected = %f.\n", computed, expected);
  // This result should be extactly the same.
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
