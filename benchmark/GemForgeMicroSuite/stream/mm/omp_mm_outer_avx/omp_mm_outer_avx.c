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

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

__attribute__((noinline)) Value foo(Value *A, Value *B, Value *C, int64_t L,
                                    int64_t M, int64_t N, int64_t m) {

  /**
   * A is LxM, B is MxN, C is LxN.
   */

  // int64_t rowBlocks = L / BLOCK_SIZE;
  // int64_t colBlocks = N / BLOCK_SIZE;
  // int64_t tailRow = rowBlocks * BLOCK_SIZE;
  // int64_t tailCol = colBlocks * BLOCK_SIZE;

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, C, L, M, N, m)
#endif
  for (int64_t l = 0; l < L; ++l) {

#pragma ss stream_name "gfm.mm_outer.A.ld"
    Value a = A[l * M + m];

    /**
     * Somehow auto-vectorized loop has an epilogue loop. Streams within this
     * epilogue loop can't be configured at outer loop, which makes the region
     * not eliminated.
     *
     * For now I will manually vectorize the loop.
     */

#ifndef NO_AVX

    const int64_t vElem = 64 / sizeof(Value);
#pragma clang unroll(disable) interleave(disable)
    for (int64_t n = 0; n < N - (vElem - 1); n += vElem) {

#pragma ss stream_name "gfm.mm_outer.B.ld"
      ValueAVX vb = ValueAVXLoad(B + m * N + n);

#pragma ss stream_name "gfm.mm_outer.C.ld"
      ValueAVX vc = ValueAVXLoad(C + l * N + n);

      ValueAVX va = ValueAVXSet1(a);

      ValueAVX v = ValueAVXAdd(vc, ValueAVXMul(va, vb));

#pragma ss stream_name "gfm.mm_outer.C.st"
      ValueAVXStore(C + l * N + n, v);
    }

    const int64_t remain = (N / vElem) * vElem;
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t n = remain; n < N; ++n) {

#pragma ss stream_name "gfm.mm_outer.ep.B.ld"
      Value b = B[m * N + n];

#pragma ss stream_name "gfm.mm_outer.ep.C.ld"
      Value c = C[l * N + n];

      Value v = c + a * b;

#pragma ss stream_name "gfm.mm_outer.ep.C.st"
      C[l * N + n] = v;
    }
#else
    // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t n = 0; n < N; ++n) {

#pragma ss stream_name "gfm.mm_outer.B.ld"
      Value b = B[m * N + n];

#pragma ss stream_name "gfm.mm_outer.C.ld"
      Value c = C[l * N + n];

      Value v = c + a * b;

#pragma ss stream_name "gfm.mm_outer.C.st"
      C[l * N + n] = v;
    }
#endif
  }

  return 0;
}

__attribute__((noinline)) Value driver(Value *A, Value *B, Value *C, int64_t L,
                                       int64_t M, int64_t N) {

  /**
   * A is LxM, B is MxN, C is LxN.
   */

  for (int64_t m = 0; m < M; ++m) {
    gf_work_begin(0);
    foo(A, B, C, L, M, N, m);
    gf_work_end(0);
  }
  return 0;
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
  volatile Value computed = driver(a, b, c, L, M, N);
  gf_detail_sim_end();

  return 0;
}
