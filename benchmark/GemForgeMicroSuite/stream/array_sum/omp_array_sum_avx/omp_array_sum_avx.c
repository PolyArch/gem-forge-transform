/**
 * Simple array sum.
 */

#include "gfm_utils.h"

#include <malloc.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef ValueT Value;

#define STRIDE 1

/**
 * Parameters:
 * STATIC_CHUNK_SIZE: OpenMP static scheduling chunk size.
 * OFFSET_BYTES:      Offset between arrays.
 */
#ifndef STATIC_CHUNK_SIZE
#define STATIC_CHUNK_SIZE 0
#endif

__attribute__((noinline)) Value foo(Value *A, uint64_t N) {
  Value ret = 0.0f;
#pragma omp parallel
  {
    ValueAVX valS = ValueAVXSet1(0.0f);
#if STATIC_CHUNK_SIZE == 0
#pragma omp for nowait schedule(static) firstprivate(A)
#else
#pragma omp for nowait schedule(static, STATIC_CHUNK_SIZE) firstprivate(A)
#endif
    for (uint64_t i = 0; i < N; i += 16) {
      ValueAVX valA = ValueAVXLoad(A + i);
      valS = ValueAVXAdd(valA, valS);
    }
    Value sum = ValueAVXReduceAdd(valS);
    __atomic_fetch_fadd(&ret, sum, __ATOMIC_RELAXED);
  }
  return ret;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t N = 16 * 1024 * 1024 / sizeof(Value);
  int check = 0;
  int warm = 1;
  int argx = 2;
  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    N = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    warm = atoi(argv[argx - 1]);
  }
  argx++;

  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB. Warm %d Check %d.\n", N * sizeof(Value) / 1024, warm,
         check);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  Value *A = (Value *)aligned_alloc(PAGE_SIZE, N * sizeof(Value));
  if (check) {
    for (int i = 0; i < N; ++i) {
      A[i] = i;
    }
  }

  Value p;
  Value *pp = &p;

  gf_detail_sim_start();

  if (warm) {
    // This should warm up the cache.
    for (long long i = 0; i < N; i += CACHE_LINE_SIZE / sizeof(Value)) {
      volatile Value x = A[i];
    }
  }
  // Start the threads.
#pragma omp parallel for schedule(static) firstprivate(pp)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = *pp;
  }

  gf_reset_stats();
  volatile Value computed = foo(A, N);
  gf_detail_sim_end();

  if (check) {
    Value expected = 0;
    for (int i = 0; i < N; i++) {
      expected += A[i];
    }
    printf("Ret = %f, Expected = %f.\n", computed, expected);
    if (fabs((computed - expected) / expected) > 0.01f) {
      gf_panic();
    }
  }

  return 0;
}
