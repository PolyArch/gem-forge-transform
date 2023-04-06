/**
 * FIR 1D filter.
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

__attribute__((noinline)) Value foo_ground_truth(Value *a, Value *weights,
                                                 Value *c, int N, int K) {

#pragma clang loop unroll(disable)
  for (int64_t k = 0; k < K; ++k) {
    Value weight = weights[k];
#pragma clang loop unroll(disable)
    for (int64_t i = 0; i < N - K; ++i) {
      c[i] += a[i + k] * weight;
    }
  }
  return 0;
}

__attribute__((noinline)) Value foo(Value *a, Value *weights, Value *c, int N,
                                    int K) {

#pragma clang loop unroll(disable)
  for (int64_t k = 0; k < K; ++k) {

#pragma ss stream_name "gfm.fir.weight.ld"
    Value weight = weights[k];
    ValueAVX valW = ValueAVXSet1(weight);

    Value *pa = a + k;

#ifndef NO_OPENMP
#if STATIC_CHUNK_SIZE == 0
#pragma omp parallel for schedule(static) firstprivate(a, valW, c)
#else
#pragma omp parallel for schedule(static, STATIC_CHUNK_SIZE)                   \
    firstprivate(a, valW, c)
#endif
#else
#pragma clang loop unroll(disable)
#endif
    for (int64_t i = 0; i < N - K; i += 64 / sizeof(Value)) {

#pragma ss stream_name "gfm.fir.A.ld"
      ValueAVX valA = ValueAVXLoad(pa + i);

#pragma ss stream_name "gfm.fir.C.ld"
      ValueAVX valC = ValueAVXLoad(c + i);

      ValueAVX valM = ValueAVXMul(valA, valW);
      ValueAVX valR = ValueAVXAdd(valM, valC);

#pragma ss stream_name "gfm.fir.C.st"
      ValueAVXStore(c + i, valR);
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t N = 16 * 1024 * 1024 / sizeof(Value);
  uint64_t K = 32;
  int check = 0;
  int warm = 0;
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
    K = atoll(argv[argx - 1]);
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
  const int vecLen = 64 / sizeof(Value);
  printf("Data size %lukB. K %lu VecLen %d.\n", N * sizeof(Value) / 1024, K,
         vecLen);
  assert(K >= vecLen && (K % vecLen == 0));
  assert(N >= K && (N % vecLen == 0));

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  const int NUM_ARRAYS = 3;
  uint64_t totalBytes = NUM_ARRAYS * (N * sizeof(Value) + OFFSET_BYTES);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *a = buffer + 0;
  Value *b = a + N + (OFFSET_BYTES / sizeof(Value));
  Value *c = b + N + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.fir.a", a, sizeof(a[0]), N);
  gf_stream_nuca_region("gfm.fir.b", b, sizeof(b[0]), K);
  gf_stream_nuca_region("gfm.fir.c", c, sizeof(c[0]), N);
  m5_stream_nuca_align(a, c, 0);
  m5_stream_nuca_remap();
#endif

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < N; i++) {
      a[i] = i;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < N; i++) {
      b[i] = i % 3;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < N; i++) {
      c[i] = 0;
    }
  }

  Value p;
  Value *pp = &p;

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.fir.a", a, sizeof(a[0]) * N);
    gf_warm_array("gfm.fir.b", b, sizeof(b[0]) * K);
    gf_warm_array("gfm.fir.c", c, sizeof(c[0]) * N);
  }
#ifndef NO_OPENMP
  startThreads(numThreads);
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, N, K);
  gf_detail_sim_end();

  if (check) {
    Value *cc = calloc(N, sizeof(Value));
    foo_ground_truth(a, b, cc, N, K);
    for (int i = 0; i < N; i++) {
      Value expected = cc[i];
      Value computed = c[i];
      if (expected != computed) {
        printf("Error at %d, Computed = %f, Expected = %f.\n", i, computed,
               expected);
        gf_panic();
      }
    }
    printf("All results match.\n");
  }

  return 0;
}
