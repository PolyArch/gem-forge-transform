#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

#include "immintrin.h"

typedef float Value;

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

const int FILTER_SIZE = 3;
const int HALF_SIZE = (FILTER_SIZE - 1) / 2;

__attribute__((noinline)) Value foo(Value *a, Value *b, int64_t M, int64_t N) {

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, M, N)
#else
#pragma clang loop unroll(disable)
#endif
  for (int64_t i = HALF_SIZE; i < M - HALF_SIZE; i++) {

#ifndef NO_AVX
    // TODO: Handle the tailing iterations.
    for (int64_t j = 0; j < N - 16; j += 16) {

      int64_t idx = i * N + j;

// First row.
#pragma ss stream_name "gfm.conv2d.A00.ld"
      __m512 valA00 = _mm512_load_ps(a + idx - N - 1);
#pragma ss stream_name "gfm.conv2d.A01.ld"
      __m512 valA01 = _mm512_load_ps(a + idx - N);
#pragma ss stream_name "gfm.conv2d.A02.ld"
      __m512 valA02 = _mm512_load_ps(a + idx - N + 1);

// Second row.
#pragma ss stream_name "gfm.conv2d.A10.ld"
      __m512 valA10 = _mm512_load_ps(a + idx - 1);
#pragma ss stream_name "gfm.conv2d.A11.ld"
      __m512 valA11 = _mm512_load_ps(a + idx);
#pragma ss stream_name "gfm.conv2d.A12.ld"
      __m512 valA12 = _mm512_load_ps(a + idx + 1);

// Third row.
#pragma ss stream_name "gfm.conv2d.A20.ld"
      __m512 valA20 = _mm512_load_ps(a + idx + N - 1);
#pragma ss stream_name "gfm.conv2d.A21.ld"
      __m512 valA21 = _mm512_load_ps(a + idx + N);
#pragma ss stream_name "gfm.conv2d.A22.ld"
      __m512 valA22 = _mm512_load_ps(a + idx + N + 1);

      __m512 coef0 = _mm512_set1_ps(0.0751f);
      __m512 coef1 = _mm512_set1_ps(0.1238f);
      __m512 coef2 = _mm512_set1_ps(0.2042f);

      __m512 valA0 = _mm512_add_ps(_mm512_add_ps(_mm512_mul_ps(coef0, valA00),
                                                 _mm512_mul_ps(coef0, valA02)),
                                   _mm512_mul_ps(coef1, valA01));
      __m512 valA1 = _mm512_add_ps(_mm512_add_ps(_mm512_mul_ps(coef1, valA10),
                                                 _mm512_mul_ps(coef1, valA12)),
                                   _mm512_mul_ps(coef2, valA11));
      __m512 valA2 = _mm512_add_ps(_mm512_add_ps(_mm512_mul_ps(coef0, valA20),
                                                 _mm512_mul_ps(coef0, valA22)),
                                   _mm512_mul_ps(coef1, valA21));

      __m512 valA = _mm512_add_ps(_mm512_add_ps(valA0, valA1), valA2);

#pragma ss stream_name "gfm.conv2d.B.st"
      _mm512_store_ps(b + idx, valA);
    }
#else

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = HALF_SIZE; j < N - HALF_SIZE; j++) {

      int64_t idx = i * N + j;

// First row.
#pragma ss stream_name "gfm.conv2d.A00.ld"
      Value valA00 = *(a + idx - N - 1);
#pragma ss stream_name "gfm.conv2d.A01.ld"
      Value valA01 = *(a + idx - N);
#pragma ss stream_name "gfm.conv2d.A02.ld"
      Value valA02 = *(a + idx - N + 1);

// Second row.
#pragma ss stream_name "gfm.conv2d.A10.ld"
      Value valA10 = *(a + idx - 1);
#pragma ss stream_name "gfm.conv2d.A11.ld"
      Value valA11 = *(a + idx);
#pragma ss stream_name "gfm.conv2d.A12.ld"
      Value valA12 = *(a + idx + 1);

// Third row.
#pragma ss stream_name "gfm.conv2d.A20.ld"
      Value valA20 = *(a + idx + N - 1);
#pragma ss stream_name "gfm.conv2d.A21.ld"
      Value valA21 = *(a + idx + N);
#pragma ss stream_name "gfm.conv2d.A22.ld"
      Value valA22 = *(a + idx + N + 1);

      Value valA0 = 0.0751f * valA00 + 0.1238f * valA01 + 0.0751f * valA02;
      Value valA1 = 0.1238f * valA10 + 0.2042f * valA11 + 0.1238f * valA12;
      Value valA2 = 0.0751f * valA20 + 0.1238f * valA21 + 0.0751f * valA22;

      Value valA = valA0 + valA1 + valA2;

#pragma ss stream_name "gfm.conv2d.B.st"
      *(b + idx) = valA;
    }
#endif
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t M = 2 * 1024;
  uint64_t N = 2 * 1024;
  int check = 0;
  int warm = 0;
  int argx = 2;

  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    M = atoll(argv[argx - 1]);
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
  printf("Data size %lukB.\n", M * N * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  const int NUM_ARRAYS = 2;
  uint64_t totalBytes = NUM_ARRAYS * (M * N * sizeof(Value) + OFFSET_BYTES);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *a = buffer + 0;
  Value *b = a + M * N + (OFFSET_BYTES / sizeof(Value));

  gf_stream_nuca_region("gfm.conv2d.a", a, sizeof(a[0]), N, M);
  gf_stream_nuca_region("gfm.conv2d.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_align(a, b, 0);
  gf_stream_nuca_align(b, b, 1);
  gf_stream_nuca_align(b, b, N);
  gf_stream_nuca_remap();

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < M * N; i++) {
      a[i] = i;
    }
  }

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.conv2d.a", a, sizeof(a[0]) * N * M);
    gf_warm_array("gfm.conv2d.b", b, sizeof(b[0]) * N * M);
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, M, N);
  gf_detail_sim_end();

  if (check) {
    assert(!check && "Check not implemented yet.");
    // for (int i = 1; i + 1 < M; i++) {
    //   for (int j = 0; j + 16 < M; j += 16) {
    //     int idx = i * N + j;
    //     Value expected = a[idx] + a[idx + 2] - a[idx + 1] + a[idx + 1 + N] +
    //                      a[idx + 1 - N] - a[idx + 1] + b[idx + 1];
    //     Value computed = c[idx + 1];
    //     if ((fabs(computed - expected) / expected) > 0.01f) {
    //       printf("Error at %dx%d, Computed = %f, Expected = %f.\n", i, j,
    //              computed, expected);
    //       gf_panic();
    //     }
    //   }
    // }
    // printf("All results match.\n");
  }

  return 0;
}
