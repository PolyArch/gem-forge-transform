/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#define STRIDE 1
// #define CHECK
#define WARM_CACHE

#define N 2 * 1024 * 1024

__attribute__((noinline)) Value foo(Value *a, Value *b) {
  Value ret = 0.0f;
#pragma omp parallel
  {
    __m512 valS = _mm512_set1_ps(0.0f);
#pragma omp for nowait schedule(static)
    for (int i = 0; i < N; i += 16) {
      __m512 valA = _mm512_load_ps(a + i);
      __m512 valB = _mm512_load_ps(b + i);
      __m512 valM = _mm512_mul_ps(valA, valB);
      valS = _mm512_add_ps(valM, valS);
    }
    Value sum = _mm512_reduce_add_ps(valS);
    __atomic_fetch_fadd(&ret, sum, __ATOMIC_RELAXED);
  }
  return ret;
}

#define CACHE_BLOCK_SIZE 64
#define PAGE_SIZE (4 * 1024)

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  Value *Buffer =
      (Value *)aligned_alloc(CACHE_BLOCK_SIZE, 2 * N * sizeof(Value));
  Value *A = Buffer + 0;
  Value *B = Buffer + N;
// Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < 2 * N; i += elementsPerPage) {
    volatile Value v = Buffer[i];
  }
// We avoid AVX since we only have partial AVX support.
#pragma clang loop vectorize(disable)
  for (int i = 0; i < N; ++i) {
    A[i] = i;
    B[i] = i % 3;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  WARM_UP_ARRAY(A, N);
  WARM_UP_ARRAY(B, N);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = A[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(A, B);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    expected += A[i] * B[i];
  }
  printf("Computed = %f, Expected = %f.\n", computed, expected);
  if ((fabs(computed - expected) / expected) > 0.01f) {
    gf_panic();
  }
#endif

  return 0;
}
