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

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int64_t L,
                                    int64_t M, int64_t N) {

/**
 * Usually L is quite small. To get enough parallelism, if we use OpenMP,
 * we swap the i,j loop.
 */
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, c, L, M, N)
  for (int64_t j = 0; j < M - 2; j++) {
#pragma clang loop unroll(disable)
    for (int64_t i = 0; i < L - 2; i++) {
#else
#pragma clang loop unroll(disable)
  for (int64_t i = 0; i < L - 2; i++) {
#pragma clang loop unroll(disable)
    for (int64_t j = 0; j < M - 2; j++) {
#endif

#ifndef NO_AVX
#pragma clang loop unroll(disable) vectorize_width(16) interleave(disable)
#else
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
#endif
      for (int64_t k = 0; k < N - 2; k++) {

        int64_t idx = i * M * N + j * N + k + M * N + N + 1;

#pragma ss stream_name "gfm.stencil3d.Aw.ld"
        Value valAw = a[idx - 1];

#pragma ss stream_name "gfm.stencil3d.Ac.ld"
        Value valAc = a[idx];

#pragma ss stream_name "gfm.stencil3d.Ae.ld"
        Value valAe = a[idx + 1];

#pragma ss stream_name "gfm.stencil3d.An.ld"
        Value valAn = a[idx - N];

#pragma ss stream_name "gfm.stencil3d.As.ld"
        Value valAs = a[idx + N];

#pragma ss stream_name "gfm.stencil3d.Af.ld"
        Value valAf = a[idx - N * N];

#pragma ss stream_name "gfm.stencil3d.Ab.ld"
        Value valAb = a[idx + N * N];

#pragma ss stream_name "gfm.stencil3d.B.ld"
        Value valB = b[idx];

        Value valAx = (valAw + valAe) - valAc;
        Value valAy = (valAn + valAs) - valAc;
        Value valAz = (valAf + valAb) - valAc;
        Value valM = (valAx + valAy + valAz) + valB;

#pragma ss stream_name "gfm.stencil3d.C.st"
        c[idx] = valM;
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t M = 512;
  uint64_t N = 512;
  uint64_t L = 16;
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
    L = atoll(argv[argx - 1]);
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
  printf("Data size %lukB.\n", M * N * L * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t T = L * M * N;

  const int NUM_ARRAYS = 3;
  uint64_t totalBytes = NUM_ARRAYS * (T * sizeof(Value) + OFFSET_BYTES);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *a = buffer + 0;
  Value *b = a + T + (OFFSET_BYTES / sizeof(Value));
  Value *c = b + T + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.stencil3d.a", a, sizeof(a[0]), N, M, L);
  gf_stream_nuca_region("gfm.stencil3d.b", b, sizeof(b[0]), N, M, L);
  gf_stream_nuca_region("gfm.stencil3d.c", c, sizeof(c[0]), N, M, L);
  gf_stream_nuca_align(a, c, 0);
  gf_stream_nuca_align(b, c, 0);
  gf_stream_nuca_align(c, c, 1);
  gf_stream_nuca_align(c, c, N);
  gf_stream_nuca_align(c, c, N * M);
  gf_stream_nuca_remap();
#endif

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < T; i++) {
      a[i] = i;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < T; i++) {
      b[i] = i % 3;
    }
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < T; i++) {
      c[i] = 0;
    }
  }

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.stencil3d.a", a, sizeof(a[0]) * T);
    gf_warm_array("gfm.stencil3d.b", b, sizeof(b[0]) * T);
    gf_warm_array("gfm.stencil3d.c", c, sizeof(c[0]) * T);
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
    Value *y = b;
    for (int i = 0; i < rounds; i++) {
      volatile Value computed = foo(x, b, y, L, M, N);
      Value *t = x;
      x = y;
      y = t;
    }
  }
  gf_detail_sim_end();

  if (check) {
    assert(0 && "No check.");
    printf("All results match.\n");
  }

  return 0;
}
