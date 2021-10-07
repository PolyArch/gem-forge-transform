/**
 * Simple array sum.
 */
#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

#include <malloc.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

// We sum over 16MB data. 16M / sizeof(Value)
#define N 4194304
#define V 16

__attribute__((noinline)) Value foo(Value *A) {
  // #pragma clang loop vectorize(disable)
  Value ret = 0.0f;
#pragma omp parallel
  {
    // __m512 valS = _mm512_set1_ps(0.0f);
    Value sum = 0.0f;
#pragma omp for schedule(static)
    for (uint64_t i = 0; i < N; i += 16) {
      // __m512 valA = _mm512_load_ps(A + i);
      // valS = _mm512_add_ps(valA, valS);
      sum += A[i];
    }
    // Value sum = _mm512_reduce_add_ps(valS);
    __atomic_fetch_fadd(&ret, sum, __ATOMIC_RELAXED);
  }
  return ret;
}

#define CACHE_BLOCK_SIZE 64

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  Value *A = (Value *)aligned_alloc(CACHE_BLOCK_SIZE, N * sizeof(Value));

#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i += CACHE_BLOCK_SIZE / sizeof(Value)) {
    volatile Value x = A[i];
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = A[tid];
  }
#endif
#ifdef GEM_FORGE
  m5_reset_stats(0, 0);
#endif

  volatile Value c = foo(A);
#ifdef GEM_FORGE
  m5_detail_sim_end();
  exit(0);
#endif

#ifdef CHECK
  // Value expected = 0;
  // for (int i = 0; i < N; i += STRIDE) {
  //   expected += a[i];
  // }
  // expected *= NUM_THREADS;
  // printf("Ret = %d, Expected = %d.\n", c, expected);
#endif

  return 0;
}
