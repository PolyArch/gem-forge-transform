/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
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

#ifndef OFFSET_BYTES
#define OFFSET_BYTES 0
#endif

__attribute__((noinline)) Value foo(Value *a, Value *b, uint64_t N) {
  Value ret = 0.0f;
#pragma omp parallel
  {
    ValueAVX valS = ValueAVXSet1(0.0f);
#if STATIC_CHUNK_SIZE == 0
#pragma omp for nowait schedule(static) firstprivate(a, b)
#else
#pragma omp for nowait schedule(static, STATIC_CHUNK_SIZE)            \
    firstprivate(a, b)
#endif
    for (int i = 0; i < N; i += 16) {
      ValueAVX valA = ValueAVXLoad(a + i);
      ValueAVX valB = ValueAVXLoad(b + i);
      ValueAVX valM = ValueAVXMul(valA, valB);
      valS = ValueAVXAdd(valM, valS);
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
  printf("Data size %lukB.\n", N * sizeof(Value) / 1024);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  const int NUM_ARRAYS = 2;
  uint64_t totalBytes = NUM_ARRAYS * (N * sizeof(Value) + OFFSET_BYTES);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;

  int *idx = (int *)aligned_alloc(CACHE_LINE_SIZE, numPages * sizeof(int));
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; ++i) {
    idx[i] = i;
  }
#ifdef RANDOMIZE
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int j = numPages - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    int tmp = idx[i];
    idx[i] = idx[j];
    idx[j] = tmp;
  }
#endif

  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Now we touch all the pages according to the index.
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    int pageIdx = idx[i];
    int elementIdx = pageIdx * elementsPerPage;
    volatile Value v = buffer[elementIdx];
  }

// We avoid AVX since we only have partial AVX support.
#ifdef REVERSE
  Value *B = buffer + 0;
  Value *A = B + N + (OFFSET_BYTES / sizeof(Value));
#else
  Value *A = buffer + 0;
  Value *B = A + N + (OFFSET_BYTES / sizeof(Value));
#endif

  printf("A %p B %p N %ld ElementSize %lu.\n", A, B, N, sizeof(A[0]));
  gf_stream_nuca_region("gfm.dot_prod.A", A, sizeof(Value), N);
  gf_stream_nuca_region("gfm.dot_prod.A", B, sizeof(Value), N);
  gf_stream_nuca_align(A, B, 0);
  gf_stream_nuca_remap();

  if (check) {
#pragma clang loop vectorize(disable)
    for (int i = 0; i < N; ++i) {
      A[i] = i;
      B[i] = i % 3;
    }
  }

  Value p;
  Value *pp = &p;

  gf_detail_sim_start();
  if (warm) {
    // This should warm up the cache.
    WARM_UP_ARRAY(A, N);
    WARM_UP_ARRAY(B, N);
  }
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = *pp;
  }

  gf_reset_stats();
  volatile Value computed = foo(A, B, N);
  gf_detail_sim_end();

  if (check) {
    Value expected = 0;
    for (int i = 0; i < N; i += STRIDE) {
      expected += A[i] * B[i];
    }
    printf("Computed = %f, Expected = %f.\n", computed, expected);
    if ((fabs(computed - expected) / expected) > 0.01f) {
      gf_panic();
    }
  }

  return 0;
}
