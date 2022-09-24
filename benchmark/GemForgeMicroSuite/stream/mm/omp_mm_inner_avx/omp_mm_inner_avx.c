#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

#define LOOP_LNM 1
#define LOOP_NLM 2

#ifndef LOOP_ORDER
#define LOOP_ORDER LOOP_LNM
#endif

#include "immintrin.h"

typedef ValueT Value;
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

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 16
#endif

__attribute__((noinline)) Value foo(Value *A, Value *B, Value *Bt, Value *C,
                                    int64_t L, int64_t M, int64_t N,
                                    int64_t P) {

  /**
   * A is LxM, B is MxN, C is LxN.
   * First transpose B.
   */

  __builtin_assume(L > 0);
  __builtin_assume(M > 0);
  __builtin_assume(N > 0);
  __builtin_assume(P > 0);

  gf_work_begin(0);

  // #ifndef NO_OPENMP
  // #pragma omp parallel for schedule(static) firstprivate(B, Bt, M, N)
  // #else
  // #pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
  // #endif
  //   for (int64_t n = 0; n < N; ++n) {
  // #pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
  //     for (int64_t m = 0; m < M; ++m) {
  //       Bt[n * M + m] = B[m * M + n];
  //     }
  //   }
  gf_work_end(0);

  // int64_t rowBlocks = L / BLOCK_SIZE;
  // int64_t colBlocks = N / BLOCK_SIZE;
  // int64_t tailRow = rowBlocks * BLOCK_SIZE;
  // int64_t tailCol = colBlocks * BLOCK_SIZE;

  gf_work_begin(1);

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, Bt, C, L, M, N, P)
#endif

#if LOOP_ORDER == LOOP_LNM

  for (int64_t l = 0; l < P; ++l) {
    for (int64_t n = 0; n < N; ++n) {

#elif LOOP_ORDER == LOOP_NLM

  for (int64_t n = 0; n < P; ++n) {
    for (int64_t l = 0; l < L; ++l) {

#else
#error "Unkown LoopOrder"
#endif

      Value s = 0;

#ifndef NO_AVX
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t m = 0; m < M; ++m) {

#if LOOP_ORDER == LOOP_LNM
#pragma ss stream_name "gfm.mm_inner_lnm.A.ld"
#else
#pragma ss stream_name "gfm.mm_inner_nlm.A.ld"
#endif
        Value a = A[l * M + m];

#if LOOP_ORDER == LOOP_LNM
#pragma ss stream_name "gfm.mm_inner_lnm.Bt.ld"
#else
#pragma ss stream_name "gfm.mm_inner_nlm.Bt.ld"
#endif
        Value b = Bt[n * M + m];

        s += a * b;
      }

#pragma ss stream_name "gfm.mm_inner.C.st"
      C[l * N + n] = s;
    }
  }

  gf_work_end(1);

  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t L = 4 * 1024 / sizeof(Value);
  uint64_t M = 4 * 1024 / sizeof(Value);
  uint64_t N = 4 * 1024 / sizeof(Value);
  uint64_t P = 64;
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
  assert(L >= 64);
  assert(M >= 64);
  assert(N >= 64);
  assert(P >= 1);
  uint64_t T = 2 * M * N + L * N + L * M;
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
  Value *bt = b + M * N;
  Value *c = bt + N * M + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.mm_inner.a", a, sizeof(a[0]), M, L);
  gf_stream_nuca_region("gfm.mm_inner.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_region("gfm.mm_inner.bt", bt, sizeof(bt[0]), M, N);
  gf_stream_nuca_region("gfm.mm_inner.c", c, sizeof(c[0]), N, L);
  gf_stream_nuca_align(b, a, 0);
  gf_stream_nuca_align(bt, a, 0);
  gf_stream_nuca_align(c, a, 0);
  gf_stream_nuca_align(a, a, 1);
  gf_stream_nuca_align(a, a, M);
  gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT, 1);
#ifdef PUM_TILE_DIM0_ELEMS
  gf_stream_nuca_set_property(a, STREAM_NUCA_REGION_PROPERTY_PUM_TILE_SIZE_DIM0,
                              PUM_TILE_DIM0_ELEMS);
  gf_stream_nuca_set_property(
      bt, STREAM_NUCA_REGION_PROPERTY_PUM_TILE_SIZE_DIM0, PUM_TILE_DIM0_ELEMS);
  gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_PUM_TILE_SIZE_DIM0,
                              PUM_TILE_DIM0_ELEMS);
#endif
  // gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
  gf_stream_nuca_remap();
#endif

  if (check) {
    for (int l = 0; l < L; ++l) {
#pragma clang loop vectorize(disable)
      for (int m = 0; m < M; ++m) {
        int idx = l * M + m;
        a[idx] = idx % 17;
      }
    }
    for (int m = 0; m < M; ++m) {
#pragma clang loop vectorize(disable)
      for (int n = 0; n < N; ++n) {
        int idx = m * N + n;
        b[idx] = idx % 13;
        bt[n * M + m] = idx % 13;
      }
    }
  }

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.mm_inner.a", a, L * M * sizeof(a[0]));
    gf_warm_array("gfm.mm_inner.b", b, M * N * sizeof(b[0]));
    gf_warm_array("gfm.mm_inner.bt", bt, N * M * sizeof(bt[0]));
    gf_warm_array("gfm.mm_inner.c", c, L * N * sizeof(c[0]));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, bt, c, L, M, N, P);
  gf_detail_sim_end();

  if (check) {
    for (int l = 0; l < P; ++l) {
      for (int n = 0; n < N; ++n) {
        Value computed = c[l * N + n];
        Value truth = 0;
#pragma clang loop vectorize(disable)
        for (int m = 0; m < M; ++m) {
          truth += a[l * M + m] * b[m * N + n];
        }
        if (fabs(computed - truth) > 0.0001f) {
          printf("Mismatch in Result %dx%d Computed %f Truth %f.\n", l, n,
                 computed, truth);
          gf_panic();
        }
      }
    }
    printf("All results match.\n");
  }

  return 0;
}
