#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

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

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int M,
                                    int N) {

#if defined(TILE_ROW) && defined(TILE_COL)

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, c, M, N)
#else
#pragma clang loop unroll(disable)
#endif
  for (int64_t ii = 1; ii < M - 1; ii += TILE_ROW) {
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
    for (int64_t jj = 1; jj < N - 1; jj += TILE_COL) {

      int64_t iEnd = (ii + TILE_ROW) < (M - 1) ? (ii + TILE_ROW) : (M - 1);
      int64_t jEnd = (jj + TILE_COL) < (N - 1) ? (jj + TILE_COL) : (N - 1);

#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
      for (int64_t i = ii; i < iEnd; ++i) {

#ifndef NO_AVX
#pragma clang loop unroll(disable) vectorize_width(16) interleave(disable)
#else
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
#endif
        for (int64_t j = jj; j < jEnd; ++j) {

          int64_t idx = i * N + j;

          Value valAw = a[idx - 1];
          Value valAc = a[idx];
          Value valAe = a[idx + 1];
          Value valAn = a[idx - N];
          Value valAs = a[idx + N];
          Value valB = b[idx];

#ifdef RODINIA_HOTSPOT_KERNEL

#if VALUE_TYPE == VALUE_TYPE_FLOAT
          const Value Cap = 0.5;
          const Value Rx = 1.5;
          const Value Ry = 1.2;
          const Value Rz = 0.7;
          const Value ambTemp = 80.0;
#else
          const Value Cap = 3;
          const Value Rx = 2;
          const Value Ry = 2;
          const Value Rz = 2;
          const Value ambTemp = 80;
#endif
          Value delta = Cap * (valB + (valAs + valAn - 2.f * valAc) * Ry +
                               (valAe + valAw - 2.f * valAc) * Rx +
                               (ambTemp - valAc) * Rz);
          Value valM = valAc + delta;

#else
          Value valAh = (valAw + valAe) - valAc;
          Value valAv = (valAn + valAs) - valAc;
          Value valM = (valAh + valAv) - valB;
#endif

          c[idx] = valM;
        }
      }
    }
  }

#else

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, c, M, N)
#else
#pragma clang loop unroll(disable)
#endif
  for (int64_t i = 1; i < M - 1; i++) {

#ifndef NO_AVX
#pragma clang loop unroll(disable) vectorize_width(16) interleave(disable)
#else
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
#endif
    for (int64_t j = 1; j < N - 1; j++) {

      int64_t idx = i * N + j;

#pragma ss stream_name "gfm.stencil2d.Aw.ld"
      Value valAw = a[idx - 1];

#pragma ss stream_name "gfm.stencil2d.Ac.ld"
      Value valAc = a[idx];

#pragma ss stream_name "gfm.stencil2d.Ae.ld"
      Value valAe = a[idx + 1];

#pragma ss stream_name "gfm.stencil2d.An.ld"
      Value valAn = a[idx - N];

#pragma ss stream_name "gfm.stencil2d.As.ld"
      Value valAs = a[idx + N];

#pragma ss stream_name "gfm.stencil2d.B.ld"
      Value valB = b[idx];

#ifdef RODINIA_HOTSPOT_KERNEL

#if VALUE_TYPE == VALUE_TYPE_FLOAT
      const Value Cap = 0.5;
      const Value Rx = 1.5;
      const Value Ry = 1.2;
      const Value Rz = 0.7;
      const Value ambTemp = 80.0;
#else
      const Value Cap = 3;
      const Value Rx = 2;
      const Value Ry = 2;
      const Value Rz = 2;
      const Value ambTemp = 80;
#endif
      Value delta =
          Cap * (valB + (valAs + valAn - 2.f * valAc) * Ry +
                 (valAe + valAw - 2.f * valAc) * Rx + (ambTemp - valAc) * Rz);
      Value valM = valAc + delta;

#else
      Value valAh = (valAw + valAe) - valAc;
      Value valAv = (valAn + valAs) - valAc;
      Value valM = (valAh + valAv) - valB;
#endif

#pragma ss stream_name "gfm.stencil2d.C.st"
      c[idx] = valM;
    }
  }

#endif
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t M = 2 * 1024;
  uint64_t N = 2 * 1024;
  int rounds = 1;
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
    rounds = atoi(argv[argx - 1]);
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
  printf("Threads: %d. Rounds: %d.\n", numThreads, rounds);
  printf("Data size %lukB.\n", M * N * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  const int NUM_ARRAYS = 3;
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
  Value *c = b + M * N + (OFFSET_BYTES / sizeof(Value));

  gf_stream_nuca_region("gfm.stencil2d.a", a, sizeof(a[0]), N, M);
  gf_stream_nuca_region("gfm.stencil2d.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_region("gfm.stencil2d.c", c, sizeof(c[0]), N, M);
  gf_stream_nuca_align(a, c, 0);
  gf_stream_nuca_align(b, c, 0);
  gf_stream_nuca_align(c, c, 1);
  gf_stream_nuca_align(c, c, N);
  gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT, 1);
  gf_stream_nuca_remap();

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < M * N; i++) {
      a[i] = i;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < M * N; i++) {
      b[i] = i % 3;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < M * N; i++) {
      c[i] = 0;
    }
  }

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.stencil2d.a", a, sizeof(a[0]) * N * M);
    gf_warm_array("gfm.stencil2d.b", b, sizeof(b[0]) * N * M);
    gf_warm_array("gfm.stencil2d.c", c, sizeof(c[0]) * N * M);
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  {
    Value *x = a;
    Value *y = c;
    for (int i = 0; i < rounds; ++i) {
      volatile Value computed = foo(x, b, y, M, N);
      // Swap x and y.
      Value *t = y;
      y = x;
      x = t;
    }
  }
  gf_detail_sim_end();

  if (check) {
    for (int i = 1; i + 1 < M; i++) {
      for (int j = 0; j + 16 < M; j += 16) {
        int idx = i * N + j;
        Value expected = a[idx] + a[idx + 2] - a[idx + 1] + a[idx + 1 + N] +
                         a[idx + 1 - N] - a[idx + 1] + b[idx + 1];
        Value computed = c[idx + 1];
        if ((fabs(computed - expected) / expected) > 0.01f) {
          printf("Error at %dx%d, Computed = %f, Expected = %f.\n", i, j,
                 computed, expected);
          gf_panic();
        }
      }
    }
    printf("All results match.\n");
  }

  return 0;
}
