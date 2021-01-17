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
// #define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  #pragma omp parallel for schedule(static) firstprivate(a, b, c)
  for (int i = 0; i < N; i += 16) {
    __m512 valA = _mm512_load_ps(a + i);
    __m512 valB = _mm512_load_ps(b + i);
    __m512 valM = _mm512_add_ps(valA, valB);
    _mm512_store_ps(c + i, valM);
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 2 * 1024 * 1024;

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  Value *a = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  Value *b = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  Value *c = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));

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

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(b, N);
  WARM_UP_ARRAY(c, N);
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(a, b, c, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < N; i += STRIDE) {
    Value expected = a[i] + b[i];
    Value computed = c[i];
    if ((fabs(computed - expected) / expected) > 0.01f) {
      printf("Error at %d, A %f B %f C %f, Computed = %f, Expected = %f.\n", i,
             a[i], b[i], c[i], computed, expected);
      gf_panic();
    }
  }
  printf("All results match.\n");
#endif

  return 0;
}
