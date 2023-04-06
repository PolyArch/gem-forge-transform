/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

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

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
#ifndef NO_OPENMP
#if STATIC_CHUNK_SIZE == 0
#pragma omp parallel for schedule(static) firstprivate(a, b, c)
#else
#pragma omp parallel for schedule(static, STATIC_CHUNK_SIZE)                   \
    firstprivate(a, b, c)
#endif
#else
#pragma clang loop unroll(disable)
#endif
  for (int64_t i = 0; i < N; i += 64 / sizeof(ValueT)) {

#pragma ss stream_name "gfm.vec_add.A.ld"
    ValueAVX valA = ValueAVXLoad(a + i);

#pragma ss stream_name "gfm.vec_add.B.ld"
    ValueAVX valB = ValueAVXLoad(b + i);

    ValueAVX valM = ValueAVXAdd(valA, valB);

#pragma ss stream_name "gfm.vec_add.C.st"
    ValueAVXStore(c + i, valM);
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t offsetBytes = 0;
  uint64_t N = 16 * 1024 * 1024 / sizeof(Value);
  int check = 0;
  int warm = 0;
  int argx = 2;
  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
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
  if (argc >= argx) {
    offsetBytes = atoll(argv[argx - 1]);
  }
  argx++;
  int random = 0;
  if (offsetBytes == -1) {
    random = 1;
    offsetBytes = 0;
  }
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB Offset %lukB Random %d.\n", N * sizeof(Value) / 1024,
         offsetBytes / 1024, random);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  const int NUM_ARRAYS = 3;
  uint64_t totalBytes = NUM_ARRAYS * (N * sizeof(Value) + offsetBytes);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = NULL;
  if (random) {
    buffer = alignedAllocAndRandomTouch(numPages, PAGE_SIZE);
  } else {
    buffer = alignedAllocAndTouch(numPages, PAGE_SIZE);
  }

  // C is the middle one as a/b is sending to c.
  Value *a = buffer + 0;
  Value *c = a + N + (offsetBytes / sizeof(Value));
  Value *b = c + N + (offsetBytes / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.vec_add.a", a, sizeof(a[0]), N);
  gf_stream_nuca_region("gfm.vec_add.b", b, sizeof(b[0]), N);
  gf_stream_nuca_region("gfm.vec_add.c", c, sizeof(c[0]), N);
  if (!random) {
    m5_stream_nuca_align(a, c, 0);
    m5_stream_nuca_align(b, c, 0);
    m5_stream_nuca_remap();
  }
#endif

  if (check) {
#pragma clang loop vectorize(disable)
    for (long long i = 0; i < N; i++) {
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

  Value p;
  Value *pp = &p;

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.vec_add.a", a, sizeof(a[0]) * N);
    gf_warm_array("gfm.vec_add.b", b, sizeof(b[0]) * N);
    gf_warm_array("gfm.vec_add.c", c, sizeof(c[0]) * N);
  }
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(pp)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = *pp;
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, N);
  gf_detail_sim_end();

  if (check) {
    for (int i = 0; i < N; i += STRIDE) {
      Value expected = a[i] + b[i];
      Value computed = c[i];
      if ((fabs(computed - expected) / expected) > 0.01f) {
        printf("Error at %d, A %f B %f C %f, Computed = %f, Expected = %f.\n",
               i, a[i], b[i], c[i], computed, expected);
        gf_panic();
      }
    }
    printf("All results match.\n");
  }

  return 0;
}
