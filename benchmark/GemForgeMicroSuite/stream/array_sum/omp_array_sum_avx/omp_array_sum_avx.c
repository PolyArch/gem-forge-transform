/**
 * Simple array sum.
 */

#include "gfm_utils.h"

#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

#include "immintrin.h"

typedef ValueT Value;

#ifdef SPLIT_2D
const int64_t nSplits = 64;
Value partials[nSplits];
#endif

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
#ifndef NO_OPENMP

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

#else

  // No OpenMP version.

#ifdef SPLIT_2D

  const int64_t elemsPerThread = N / nSplits;

  __builtin_assume(N >= nSplits);
  __builtin_assume(N % nSplits == 0);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t t = 0; t < nSplits; ++t) {

    Value partial = 0;

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t i = 0; i < elemsPerThread; ++i) {
      partial += A[t * elemsPerThread + i];
    }

    partials[t] = partial;
    // ret += partial;
  }

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t t = 0; t < nSplits; ++t) {
    ret += partials[t];
  }

#else

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t i = 0; i < N; ++i) {
    ret += A[i];
  }

#endif

#endif
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

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  Value *A = (Value *)aligned_alloc(PAGE_SIZE, N * sizeof(Value));

  uint64_t totalBytes = N * sizeof(Value);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = A[i * elementsPerPage];
  }

  if (check) {
    for (int i = 0; i < N; ++i) {
      A[i] = i;
    }
  }

  Value p;
  Value *pp = &p;

#ifdef SPLIT_2D
  gf_stream_nuca_region("gfm.array_sum.A", A, sizeof(Value), N / nSplits,
                        nSplits);
  gf_stream_nuca_region("gfm.array_sum.partials", partials, sizeof(Value),
                        nSplits);
#else
  gf_stream_nuca_region("gfm.array_sum.A", A, sizeof(Value), N);
#endif
  gf_stream_nuca_remap();

  gf_detail_sim_start();

  if (warm) {
    gf_warm_array("gfm.array_sum.A", A, N * sizeof(Value));
  }

#ifdef SPLIT_2D
  for (int i = 0; i < nSplits; ++i) {
    partials[i] = 0;
  }
#endif

  // Start the threads.
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(pp)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = *pp;
  }
#endif

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
