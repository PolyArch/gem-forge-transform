#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NO_OPENMP
#include <omp.h>
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

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int64_t N) {

#ifndef NO_AVX

  ValueVec *va0 = (ValueVec *)(a);
  ValueVec *va1 = (ValueVec *)(a + 1);
  ValueVec *va2 = (ValueVec *)(a + 2);
  ValueVec *vb = (ValueVec *)(b);
  ValueVec *vc = (ValueVec *)(c);
  int64_t vN = (N - ValueVecLen - 2) / ValueVecLen;

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(va0, va1, va2, vb, vc, vN)
#else
#pragma clang loop unroll(disable) vectorize(disable)
#endif
  for (int64_t i = 0; i < vN; i++) {

#pragma ss stream_name "gfm.stencil1d.A0.ld"
    ValueAVX valA0 = ValueAVXLoad(va0 + i);

#pragma ss stream_name "gfm.stencil1d.A1.ld"
    ValueAVX valA1 = ValueAVXLoad(va1 + i);

#pragma ss stream_name "gfm.stencil1d.A2.ld"
    ValueAVX valA2 = ValueAVXLoad(va2 + i);

#pragma ss stream_name "gfm.stencil1d.B.ld"
    ValueAVX valB = ValueAVXLoad(vb + i);

    ValueAVX valA = ValueAVXSub(ValueAVXAdd(valA0, valA2), valA1);
    ValueAVX valM = ValueAVXAdd(valA, valB);

#pragma ss stream_name "gfm.stencil1d.C.st"
    ValueAVXStore(vc + i, valM);
  }

  // Handling remain iterations.
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
  for (int64_t i = vN * ValueVecLen; i < N - 2; ++i) {
    Value *vA = a + i;

    Value valA0 = vA[0];
    Value valA1 = vA[1];
    Value valA2 = vA[2];

    Value valB = b[i];
    Value valA = valA0 + valA2 - valA1;
    Value valM = valA + valB;

    c[i] = valM;
  }

#else

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, c, N)
#else
#pragma clang loop unroll(disable) vectorize(disable)
#endif
  for (int64_t i = 0; i < N - 2; i++) {

    Value *vA = a + i;

#pragma ss stream_name "gfm.stencil1d.A0.ld"
    Value valA0 = vA[0];

#pragma ss stream_name "gfm.stencil1d.A1.ld"
    Value valA1 = vA[1];

#pragma ss stream_name "gfm.stencil1d.A2.ld"
    Value valA2 = vA[2];

#pragma ss stream_name "gfm.stencil1d.B.ld"
    Value valB = b[i];

    Value valA = valA0 + valA2 - valA1;

    Value valM = valA + valB;

#pragma ss stream_name "gfm.stencil1d.C.st"
    c[i] = valM;
  }

#endif
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t N = 16 * 1024 * 1024 / sizeof(Value);
  int rounds = 1;
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
  printf("Data size %lukB.\n", N * sizeof(Value) / 1024);

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
  gf_stream_nuca_region("gfm.stencil1d.a", a, sizeof(a[0]), N);
  gf_stream_nuca_region("gfm.stencil1d.b", b, sizeof(b[0]), N);
  gf_stream_nuca_region("gfm.stencil1d.c", c, sizeof(c[0]), N);
  gf_stream_nuca_align(a, c, 0);
  gf_stream_nuca_align(b, c, 0);
  gf_stream_nuca_align(c, c, 1);
  gf_stream_nuca_remap();
#endif

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < N + 2; i++) {
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

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.stencil1d.a", a, sizeof(a[0]) * N);
    gf_warm_array("gfm.stencil1d.b", b, sizeof(b[0]) * N);
    gf_warm_array("gfm.stencil1d.c", c, sizeof(c[0]) * N);
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
      volatile Value computed = foo(x, b, y, N);
      Value *t = y;
      y = x;
      x = t;
    }
  }
  gf_detail_sim_end();

  if (check) {
    for (int i = 0; i < N; i += STRIDE) {
      Value expected = a[i] + a[i + 2] - a[i + 1] + b[i];
      Value computed = c[i];
      if ((fabs(computed - expected) / expected) > 0.01f) {
        printf("Error at %d, A %f - %f + %f + B %f = C %f, Computed = %f, "
               "Expected = %f.\n",
               i, a[i], a[i + 1], a[i + 2], b[i], c[i], computed, expected);
        gf_panic();
      }
    }
    printf("All results match.\n");
  }

  return 0;
}
