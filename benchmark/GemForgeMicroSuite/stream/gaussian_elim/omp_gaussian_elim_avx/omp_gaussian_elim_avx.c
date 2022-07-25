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

__attribute__((noinline)) Value foo(Value *A, Value *X, Value *B, int64_t M,
                                    int64_t N, int64_t k) {

  Value akk = A[k * N + k];
  Value bk = B[k];

// Here I have to be careful to avoid k + 1.
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(A, X, B, M, N, k, akk, bk)
#endif
  for (int64_t i = k; i < M - 1; ++i) {

    int64_t ik = i * N + k;

#pragma ss stream_name "gfm.gaussian_elim.aik.ld"
    Value aik = A[ik + N];

    Value multiplier = aik / akk;

    Value *pbi = B + i;

#pragma ss stream_name "gfm.gaussian_elim.bi.ld"
    Value bi = pbi[1];

#pragma ss stream_name "gfm.gaussian_elim.bi.st"
    pbi[1] = bi - bk * multiplier;

#ifndef NO_AVX
#pragma clang loop vectorize(enable)
#endif
    for (int64_t j = k; j < N - 1; ++j) {

      Value *pakj = A + (k * N + j);

#pragma ss stream_name "gfm.gaussian_elim.akj.ld"
      Value akj = pakj[1];

      int64_t ij = i * N + j + N + 1;

#pragma ss stream_name "gfm.gaussian_elim.aij.ld"
      Value aij = A[ij];

#pragma ss stream_name "gfm.gaussian_elim.aij.st"
      A[ij] -= akj * multiplier;
    }
  }

  return 0;
}

__attribute__((noinline)) Value driver(Value *A, Value *X, Value *B, int64_t M,
                                       int64_t N, int64_t P) {

  for (int64_t k = 0; k < P - 1; ++k) {
    foo(A, X, B, M, N, k);
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t M = 4 * 1024 / sizeof(Value);
  uint64_t N = 4 * 1024 / sizeof(Value);
  uint64_t P = M;
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
    P = atoll(argv[argx - 1]);
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
  printf("Threads: %d. P %lu.\n", numThreads, P);
  printf("Data size %lukB.\n", T * sizeof(Value) / 1024);
  assert(P >= 1 && P <= M);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes = ((T + N + M) * sizeof(Value) + OFFSET_BYTES);
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
  Value *x = b + M + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.gaussian.a", a, sizeof(a[0]), M, N);
  gf_stream_nuca_region("gfm.gaussian.b", b, sizeof(b[0]), M);
  gf_stream_nuca_region("gfm.gaussian.x", x, sizeof(x[0]), N);
  gf_stream_nuca_align(a, a, 1);
  gf_stream_nuca_align(a, a, N);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.gaussian.a", a, T * sizeof(a[0]));
    gf_warm_array("gfm.gaussian.b", b, M * sizeof(b[0]));
    gf_warm_array("gfm.gaussian.x", x, N * sizeof(x[0]));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = driver(a, b, x, M, N, P);
  gf_detail_sim_end();

  return 0;
}
