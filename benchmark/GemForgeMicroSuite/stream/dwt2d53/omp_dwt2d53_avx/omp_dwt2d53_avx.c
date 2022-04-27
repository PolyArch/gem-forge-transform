#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

#include "immintrin.h"

typedef float Value;
const int ValueVecLen = 16;
typedef struct {
  float vs[ValueVecLen];
} ValueVec;

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

__attribute__((noinline)) Value foo(Value *A, Value *B, int64_t M, int64_t N) {

  /**
   * TODO: Handle the mirroring boundary case.
   */

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, M, N)
#endif
  for (int64_t i = 1; i < M - 1; i += 2) {

#ifndef NO_AVX
    // Vectorized version.
    const int64_t vElem = 64 / sizeof(Value);
    for (int64_t j = 0; j < N - (vElem - 1); j += vElem) {
#pragma ss stream_name "gfm.dwt2d53.pred_row.a1.ld"
      __m512 a1 = _mm512_load_ps(A + (i - 1) * N + j);
#pragma ss stream_name "gfm.dwt2d53.pred_row.a2.ld"
      __m512 a2 = _mm512_load_ps(A + (i + 1) * N + j);
#pragma ss stream_name "gfm.dwt2d53.pred_row.a.ld"
      __m512 a = _mm512_load_ps(A + i * N + j);

      __m512 d = _mm512_set1_ps(2.0f);
      __m512 v = _mm512_sub_ps(a, _mm512_div_ps(_mm512_add_ps(a1, a2), d));

#pragma ss stream_name "gfm.dwt2d53.pred_row.a.st"
      _mm512_store_ps(A + i * N + j, v);
    }

#ifndef NO_EPILOGUE
    const int64_t remain = (N / vElem) * vElem;
#pragma clang loop vectorize(disable)
    for (int64_t j = remain; j < N; j++) {
      Value a1 = A[(i - 1) * N + j];
      Value a2 = A[(i + 1) * N + j];
      Value a = A[i * N + j];
      A[i * N + j] = a - (a1 + a2) / 2;
    }
#endif

#else
    // Scalar version.
    for (int64_t j = 0; j < N; ++j) {
#pragma ss stream_name "gfm.dwt2d53.pred_row.a1.ld"
      Value a1 = A[(i - 1) * N + j];
#pragma ss stream_name "gfm.dwt2d53.pred_row.a2.ld"
      Value a2 = A[(i + 1) * N + j];
#pragma ss stream_name "gfm.dwt2d53.pred_row.a.ld"
      Value a = A[i * N + j];
#pragma ss stream_name "gfm.dwt2d53.pred_row.a.st"
      A[i * N + j] = a - (a1 + a2) / 2;
    }
#endif
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, M, N)
#endif
  for (int64_t i = 2; i < M - 1; i += 2) {

#ifndef NO_AVX

    // Vectorized version.
    const int64_t vElem = 64 / sizeof(Value);
    for (int64_t j = 0; j < N - (vElem - 1); j += vElem) {
#pragma ss stream_name "gfm.dwt2d53.update_row.a1.ld"
      __m512 a1 = _mm512_load_ps(A + (i - 1) * N + j);
#pragma ss stream_name "gfm.dwt2d53.update_row.a2.ld"
      __m512 a2 = _mm512_load_ps(A + (i + 1) * N + j);
#pragma ss stream_name "gfm.dwt2d53.update_row.a.ld"
      __m512 a = _mm512_load_ps(A + i * N + j);

      __m512 d = _mm512_set1_ps(4.0f);
      __m512 o = _mm512_set1_ps(2.0f);
      __m512 v = _mm512_add_ps(
          a, _mm512_div_ps(_mm512_add_ps(_mm512_add_ps(a1, a2), o), d));

#pragma ss stream_name "gfm.dwt2d53.update_row.a.st"
      _mm512_store_ps(A + i * N + j, v);
    }

#ifndef NO_EPILOGUE
    const int64_t remain = (N / vElem) * vElem;
#pragma clang loop vectorize(disable)
    for (int64_t j = remain; j < N; j++) {
      Value a1 = A[(i - 1) * N + j];
      Value a2 = A[(i + 1) * N + j];
      Value a = A[i * N + j];
      A[i * N + j] = a + (a1 + a2 + 2.0f) / 4.0f;
    }
#endif

#else
    // #pragma clang loop vectorize_width(16)
    for (int64_t j = 0; j < N; ++j) {
#pragma ss stream_name "gfm.dwt2d53.update_row.a1.ld"
      Value a1 = A[(i - 1) * N + j];
#pragma ss stream_name "gfm.dwt2d53.update_row.a2.ld"
      Value a2 = A[(i + 1) * N + j];
#pragma ss stream_name "gfm.dwt2d53.update_row.a.ld"
      Value a = A[i * N + j];
#pragma ss stream_name "gfm.dwt2d53.update_row.a.st"
      A[i * N + j] = a + (a1 + a2 + 2) / 4;
    }
#endif
  }

  /**
   * Although we can set vectorize_width to 8 and Clang will automatically
   * vectorize it, it uses masked_scatter instruction for the store, which can
   * not be analyzed by stream. Therefore, here we manually vectorize it.
   * ! TODO: MaskedStore requires aligned to cache line.
   */
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, M, N)
#endif
  for (int64_t i = 0; i < M; i++) {

#ifndef NO_AVX
    const int64_t vElem = 64 / sizeof(Value);
    // Vectorized version.
    for (int64_t j = 1; j < N - vElem; j += vElem) {
#pragma ss stream_name "gfm.dwt2d53.pred_col.a1.ld"
      __m512 a1 = _mm512_load_ps(A + i * N + j - 1);
#pragma ss stream_name "gfm.dwt2d53.pred_col.a2.ld"
      __m512 a2 = _mm512_load_ps(A + i * N + j + 1);
#pragma ss stream_name "gfm.dwt2d53.pred_col.a.ld"
      __m512 a = _mm512_load_ps(A + i * N + j);

      __m512 d = _mm512_set1_ps(2.0f);
      __m512 v = _mm512_sub_ps(a, _mm512_div_ps(_mm512_add_ps(a1, a2), d));

#pragma ss stream_name "gfm.dwt2d53.pred_col.a.st"
      _mm512_mask_store_ps(A + i * N + j, 0x5555, v);
    }

// Epilogue for remaining iterations.
#ifndef NO_EPILOGUE
    const int64_t remain = ((N - 2) / vElem) * vElem + 1;
#pragma clang loop vectorize(disable)
    for (int64_t j = remain; j < N - 1; j += 2) {
      Value a1 = A[i * N + j - 1];
      Value a2 = A[i * N + j + 1];
      Value a = A[i * N + j];
      A[i * N + j] = a - (a1 + a2) / 2;
    }
#endif

#else
    // Scalar version.
    // #pragma clang loop vectorize_width(8)
    for (int64_t j = 1; j < N - 1; j += 2) {
#pragma ss stream_name "gfm.dwt2d53.pred_col.a1.ld"
      Value a1 = A[i * N + j - 1];
#pragma ss stream_name "gfm.dwt2d53.pred_col.a2.ld"
      Value a2 = A[i * N + j + 1];
#pragma ss stream_name "gfm.dwt2d53.pred_col.a.ld"
      Value a = A[i * N + j];
#pragma ss stream_name "gfm.dwt2d53.pred_col.a.st"
      A[i * N + j] = a - (a1 + a2) / 2;
    }
#endif
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, M, N)
#endif
  for (int64_t i = 0; i < M; i++) {

#ifndef NO_AVX
    const int64_t vElem = 64 / sizeof(Value);
    // Vectorized version.
    for (int64_t j = 2; j < N - vElem; j += vElem) {
#pragma ss stream_name "gfm.dwt2d53.update_col.a1.ld"
      __m512 a1 = _mm512_load_ps(A + i * N + j - 1);
#pragma ss stream_name "gfm.dwt2d53.update_col.a2.ld"
      __m512 a2 = _mm512_load_ps(A + i * N + j + 1);
#pragma ss stream_name "gfm.dwt2d53.update_col.a.ld"
      __m512 a = _mm512_load_ps(A + i * N + j);

      __m512 o = _mm512_set1_ps(2.0f);
      __m512 d = _mm512_set1_ps(4.0f);
      __m512 v = _mm512_add_ps(
          a, _mm512_div_ps(_mm512_add_ps(_mm512_add_ps(a1, a2), o), d));

#pragma ss stream_name "gfm.dwt2d53.update_col.a.st"
      _mm512_mask_store_ps(A + i * N + j, 0x5555, v);
    }

    // Epilogue for remaining iterations.
#ifndef NO_EPILOGUE
    const int64_t remain = ((N - 3) / vElem) * vElem + 2;
#pragma clang loop vectorize(disable)
    for (int64_t j = remain; j < N - 1; j += 2) {
      Value a1 = A[i * N + j - 1];
      Value a2 = A[i * N + j + 1];
      Value a = A[i * N + j];
      A[i * N + j] = a + (a1 + a2 + 2.0f) / 4.0f;
    }
#endif

#else
    // Scalar version.
    // #pragma clang loop vectorize_width(8)
    for (int64_t j = 2; j < N - 1; j += 2) {
#pragma ss stream_name "gfm.dwt2d53.update_col.a1.ld"
      Value a1 = A[i * N + j - 1];
#pragma ss stream_name "gfm.dwt2d53.update_col.a2.ld"
      Value a2 = A[i * N + j + 1];
#pragma ss stream_name "gfm.dwt2d53.update_col.a.ld"
      Value a = A[i * N + j];
#pragma ss stream_name "gfm.dwt2d53.update_col.a.st"
      A[i * N + j] = a + (a1 + a2 + 2) / 4;
    }
#endif
  }

  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t M = 4 * 1024 / sizeof(Value);
  uint64_t N = 4 * 1024 / sizeof(Value);
  int check = 0;
  int warm = 0;
  int argx = 2;

  assert(sizeof(ValueVec) == sizeof(Value) * ValueVecLen &&
         "Mismatch ValueVec Size.");

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
  uint64_t T = M * N;
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB.\n", T * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes = ((T + T) * sizeof(Value) + OFFSET_BYTES);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  // A X = B
  Value *a = buffer + 0;
  Value *b = a + T + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.dwt2d53.a", a, sizeof(a[0]), N, M);
  gf_stream_nuca_region("gfm.dwt2d53.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_align(a, a, N);
  gf_stream_nuca_align(a, a, 1);
  gf_stream_nuca_align(b, a, 0);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.dwt2d53.a", a, T * sizeof(a[0]));
    gf_warm_array("gfm.dwt2d53.b", b, T * sizeof(b[0]));
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

  return 0;
}
