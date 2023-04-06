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
 */
#ifndef STATIC_CHUNK_SIZE
#define STATIC_CHUNK_SIZE 0
#endif

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int64_t L,
                                    int64_t M, int64_t N) {

/**
 * Usually L is quite small. To get enough parallelism, if we use OpenMP,
 * we fuse the i/j loop level.
 */
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(a, b, c, L, M, N)
  for (int64_t j = M + 1; j < L * M - M - 1; j++) {
#else
#pragma clang loop unroll(disable)
  for (int64_t i = 1; i < L - 1; i++) {
#pragma clang loop unroll(disable)
    for (int64_t j = 1; j < M - 1; j++) {
#endif

#ifndef NO_AVX

    const int64_t vLen = 64 / sizeof(Value);
    const int64_t vN = N / vLen;

#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
    for (int64_t k = 0; k < vN; k++) {

#ifndef NO_OPENMP
      int64_t idx = j * N + k * vLen + 1;
#else
      int64_t idx = (i * M + j) * N + k * vLen + 1;
#endif

#pragma ss stream_name "gfm.stencil3d.Aw.ld"
      ValueAVX valAw = ValueAVXLoad(&a[idx - 1]);

#pragma ss stream_name "gfm.stencil3d.Ac.ld"
      ValueAVX valAc = ValueAVXLoad(&a[idx]);

#pragma ss stream_name "gfm.stencil3d.Ae.ld"
      ValueAVX valAe = ValueAVXLoad(&a[idx + 1]);

#pragma ss stream_name "gfm.stencil3d.An.ld"
      ValueAVX valAn = ValueAVXLoad(&a[idx - N]);

#pragma ss stream_name "gfm.stencil3d.As.ld"
      ValueAVX valAs = ValueAVXLoad(&a[idx + N]);

#pragma ss stream_name "gfm.stencil3d.Af.ld"
      ValueAVX valAf = ValueAVXLoad(&a[idx - M * N]);

#pragma ss stream_name "gfm.stencil3d.Ab.ld"
      ValueAVX valAb = ValueAVXLoad(&a[idx + M * N]);

#pragma ss stream_name "gfm.stencil3d.B.ld"
      ValueAVX valB = ValueAVXLoad(&b[idx]);

#ifdef RODINIA_HOTSPOT3D_KERNEL

#if VALUE_TYPE == VALUE_TYPE_FLOAT
      const ValueAVX localCC = ValueAVXSet1(0.5);
      const ValueAVX localCE = ValueAVXSet1(0.6);
      const ValueAVX localCW = ValueAVXSet1(0.2);
      const ValueAVX localCN = ValueAVXSet1(0.3);
      const ValueAVX localCS = ValueAVXSet1(0.4);
      const ValueAVX localCT = ValueAVXSet1(0.8);
      const ValueAVX localCB = ValueAVXSet1(0.7);
      const ValueAVX localdt = ValueAVXSet1(0.1);
      const ValueAVX localCap = ValueAVXSet1(0.8);
      const ValueAVX localAmb = ValueAVXSet1(0.7);
#else
      const ValueAVX localCC = ValueAVXSet1(2);
      const ValueAVX localCE = ValueAVXSet1(3);
      const ValueAVX localCW = ValueAVXSet1(5);
      const ValueAVX localCN = ValueAVXSet1(11);
      const ValueAVX localCS = ValueAVXSet1(7);
      const ValueAVX localCT = ValueAVXSet1(9);
      const ValueAVX localCB = ValueAVXSet1(6);
      const ValueAVX localdt = ValueAVXSet1(12);
      const ValueAVX localCap = ValueAVXSet1(13);
      const ValueAVX localAmb = ValueAVXSet1(88);
#endif

      ValueAVX valM = ValueAVXAdd(
          ValueAVXAdd(ValueAVXAdd(ValueAVXAdd(ValueAVXMul(localCC, valAc),
                                              ValueAVXMul(localCW, valAw)),
                                  ValueAVXAdd(ValueAVXMul(localCE, valAe),
                                              ValueAVXMul(localCS, valAs))),
                      ValueAVXAdd(ValueAVXAdd(ValueAVXMul(localCN, valAn),
                                              ValueAVXMul(localCB, valAb)),
                                  ValueAVXAdd(ValueAVXMul(localCT, valAf),
                                              ValueAVXMul(ValueAVXDiv(localdt,
                                                                      localCap),
                                                          valB)))),
          ValueAVXMul(localCT, localAmb));
#else
      ValueAVX valAx = ValueAVXSub(ValueAVXAdd(valAw, valAe), valAc);
      ValueAVX valAy = ValueAVXSub(ValueAVXAdd(valAn, valAs), valAc);
      ValueAVX valAz = ValueAVXSub(ValueAVXAdd(valAf, valAb), valAc);
      ValueAVX valM =
          ValueAVXAdd(ValueAVXAdd(valAx, valAy), ValueAVXAdd(valAz, valB));
#endif

#pragma ss stream_name "gfm.stencil3d.C.st"
      ValueAVXStore(&c[idx], valM);
    }

#else
#pragma clang loop unroll(disable) vectorize(disable) interleave(disable)
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
        Value valAf = a[idx - M * N];

#pragma ss stream_name "gfm.stencil3d.Ab.ld"
        Value valAb = a[idx + M * N];

#pragma ss stream_name "gfm.stencil3d.B.ld"
        Value valB = b[idx];

#ifdef RODINIA_HOTSPOT3D_KERNEL

#if VALUE_TYPE == VALUE_TYPE_FLOAT
        const Value localCC = 0.5;
        const Value localCE = 0.6;
        const Value localCW = 0.2;
        const Value localCN = 0.3;
        const Value localCS = 0.4;
        const Value localCT = 0.8;
        const Value localCB = 0.7;
        const Value localdt = 0.1;
        const Value localCap = 0.8;
        const Value localAmb = 0.7;
#else
        const Value localCC = 2;
        const Value localCE = 3;
        const Value localCW = 5;
        const Value localCN = 11;
        const Value localCS = 7;
        const Value localCT = 9;
        const Value localCB = 6;
        const Value localdt = 12;
        const Value localCap = 13;
        const Value localAmb = 88;
#endif

        Value valM = localCC * valAc + localCW * valAw + localCE * valAe +
                     localCS * valAs + localCN * valAn + localCB * valAb +
                     localCT * valAf + (localdt / localCap) * valB +
                     localCT * localAmb;
#else
        Value valAx = (valAw + valAe) - valAc;
        Value valAy = (valAn + valAs) - valAc;
        Value valAz = (valAf + valAb) - valAc;
        Value valM = (valAx + valAy + valAz) + valB;
#endif

#pragma ss stream_name "gfm.stencil3d.C.st"
        c[idx] = valM;
      }
#endif

#ifdef NO_OPENMP
  }
#endif
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
  int offsetBytes = 0;
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
  if (argc >= argx) {
    offsetBytes = atoi(argv[argx - 1]);
  }
  argx++;
  int random = 0;
  if (offsetBytes == -1) {
    random = 1;
    offsetBytes = 0;
  }
  printf("Threads: %d. Rounds: %d.\n", numThreads, rounds);
  printf("Data size %lukB Offset %dkB Random %d.\n",
         M * N * L * sizeof(Value) / 1024, offsetBytes / 1024, random);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t T = L * M * N;

  const int NUM_ARRAYS = 3;
  uint64_t totalBytes = NUM_ARRAYS * (T * sizeof(Value) + offsetBytes);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = NULL;
  if (random) {
    buffer = alignedAllocAndRandomTouch(numPages, PAGE_SIZE);
  } else {
    buffer = alignedAllocAndTouch(numPages, PAGE_SIZE);
  }

  Value *a = buffer + 0;
  Value *b = a + T + (offsetBytes / sizeof(Value));
  Value *c = b + T + (offsetBytes / sizeof(Value));

  gf_stream_nuca_region("gfm.stencil3d.a", a, sizeof(a[0]), N, M, L);
  gf_stream_nuca_region("gfm.stencil3d.b", b, sizeof(b[0]), N, M, L);
  gf_stream_nuca_region("gfm.stencil3d.c", c, sizeof(c[0]), N, M, L);
  if (!random) {
    gf_stream_nuca_align(a, c, 0);
    gf_stream_nuca_align(b, c, 0);
    gf_stream_nuca_align(c, c, 1);
    gf_stream_nuca_align(c, c, N);
    gf_stream_nuca_align(c, c, N * M);
    gf_stream_nuca_remap();
  }

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
    Value *y = c;
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
