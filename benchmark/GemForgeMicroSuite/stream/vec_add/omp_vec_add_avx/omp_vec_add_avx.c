/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef float Value;

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
  for (int64_t i = 0; i < N; i += 16) {

#pragma ss stream_name "gfm.vec_add.A.ld"
    __m512 valA = _mm512_load_ps(a + i);

#pragma ss stream_name "gfm.vec_add.B.ld"
    __m512 valB = _mm512_load_ps(b + i);

    __m512 valM = _mm512_add_ps(valA, valB);

#pragma ss stream_name "gfm.vec_add.C.st"
    _mm512_store_ps(c + i, valM);
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
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
  printf("Number of Threads: %d.\n", numThreads);
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
  m5_stream_nuca_region("gfm.omp_vec_add_avx.a", a, sizeof(a[0]), N);
  m5_stream_nuca_region("gfm.omp_vec_add_avx.b", b, sizeof(b[0]), N);
  m5_stream_nuca_region("gfm.omp_vec_add_avx.c", c, sizeof(c[0]), N);
  m5_stream_nuca_align(a, c, 0);
  m5_stream_nuca_align(b, c, 0);
  m5_stream_nuca_remap();
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
    WARM_UP_ARRAY(a, N);
    WARM_UP_ARRAY(b, N);
    WARM_UP_ARRAY(c, N);
  }
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(pp)
#endif
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = *pp;
  }

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
