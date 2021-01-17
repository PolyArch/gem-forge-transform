/**
 * Simple array dot prod with locality control.
 * Since gem5 does demand paging, we can control how two arrays are mapped to
 * LLC banks by tweaking the order of their first accesses.
 * Assuming 1kB interleaving in LLC, with 8x8 banks, if we interleave first
 * accesses to 64kB, then we should be able to make them anligned in the same
 * LLC bank.
 */
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

#ifndef OFFSET_BYTES
#define OFFSET_BYTES 0
#endif

#define STRIDE 1
// #define CHECK
#define WARM_CACHE

#define N 2 * 1024 * 1024

__attribute__((noinline)) Value foo(Value *a, Value *b)
{
  Value ret = 0.0f;
#pragma omp parallel
  {
    __m512 valS = _mm512_set1_ps(0.0f);
#pragma omp for schedule(static)
    for (int i = 0; i < N; i += 16)
    {
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

int main(int argc, char *argv[])
{

  int numThreads = 1;
  if (argc == 2)
  {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  // Since the array it self is aligned to the LLC interleave granularity,
  // we simply make the first array OFFSET_BYTES larger.
  int totalBytes = 2 * N * sizeof(Value) + OFFSET_BYTES;
  int numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  int *idx = (int *)aligned_alloc(CACHE_BLOCK_SIZE, numPages * sizeof(int));
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; ++i)
  {
    idx[i] = i;
  }
#ifdef RANDOMIZE
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int j = numPages - 1; j > 0; --j)
  {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    int tmp = idx[i];
    idx[i] = idx[j];
    idx[j] = tmp;
  }
#endif
  Value *Buffer = (Value *)aligned_alloc(CACHE_BLOCK_SIZE, numPages * PAGE_SIZE);
  Value *A = Buffer + 0;
  Value *B = Buffer + N + (OFFSET_BYTES / sizeof(Value));

  // Now we touch all the pages according to the index.
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++)
  {
    int pageIdx = idx[i];
    int elementIdx = pageIdx * elementsPerPage;
    volatile Value v = Buffer[elementIdx];
  }

  // Initialize the array. We avoid AVX since we only have partial AVX support.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < N; i++)
  {
    A[i] = i;
    B[i] = i % 3;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i += CACHE_BLOCK_SIZE / sizeof(Value))
  {
    volatile Value x = A[i];
    volatile Value y = B[i];
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid)
  {
    volatile Value x = A[tid];
  }
#endif
  gf_reset_stats();

#ifdef REVERSE
  volatile Value computed = foo(B, A);
#else
  volatile Value computed = foo(A, B);
#endif
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE)
  {
    expected += A[i] * B[i];
  }
  printf("Computed = %f, Expected = %f.\n", computed, expected);
  if ((fabs(computed - expected) / expected) > 0.01f)
  {
    gf_panic();
  }
#endif

  return 0;
}
