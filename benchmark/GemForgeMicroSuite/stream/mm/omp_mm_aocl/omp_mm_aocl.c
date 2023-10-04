#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "cblas.h"

#ifndef NO_OPENMP
#include <omp.h>
#endif

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

#define BILLION 1000000000L

__attribute__((noinline)) uint64_t foo(Value *restrict A, Value *restrict B,
                                       Value *restrict C, int64_t M, int64_t K,
                                       int64_t N) {

  int lda = K;
  int ldb = N;
  int ldc = N;
  float alpha = 1.0;
  float beta = 0.0;

#ifndef GEM_FORGE
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
#endif

  cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, M, N, K, alpha, A, lda,
              B, ldb, beta, C, ldc);

#ifndef GEM_FORGE
  clock_gettime(CLOCK_MONOTONIC, &end);
  uint64_t diff =
      BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
  return diff;
#else
  return 0;
#endif
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t L = 4 * 1024 / sizeof(Value);
  uint64_t M = 4 * 1024 / sizeof(Value);
  uint64_t N = 4 * 1024 / sizeof(Value);
  int check = 0;
  int warm = 0;
  int argx = 2;

  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    L = atoll(argv[argx - 1]);
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
  uint64_t T = M * N + L * N + L * M;
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB.\n", T * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes = T * sizeof(Value) + OFFSET_BYTES;
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  // A X = B
  Value *a = buffer;
  Value *b = a + L * M;
  Value *c = b + M * N + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.mm_outer.a", a, sizeof(a[0]), M, L);
  gf_stream_nuca_region("gfm.mm_outer.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_region("gfm.mm_outer.c", c, sizeof(c[0]), N, L);
  gf_stream_nuca_align(a, a, 1);
  gf_stream_nuca_align(a, a, M);
  gf_stream_nuca_align(b, a, 0);
  gf_stream_nuca_align(c, a, 0);
  gf_stream_nuca_set_property(a, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_set_property(b, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.mm_outer.a", a, L * M * sizeof(a[0]));
    gf_warm_array("gfm.mm_outer.b", b, M * N * sizeof(b[0]));
    gf_warm_array("gfm.mm_outer.c", c, L * N * sizeof(c[0]));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, L, M, N);
  gf_detail_sim_end();

  return 0;
}
