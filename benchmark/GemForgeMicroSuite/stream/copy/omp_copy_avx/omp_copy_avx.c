/**
 * Simple coalesced sum copy.
 */
#include "gfm_utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef int Value;

// #define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int N) {
#pragma omp parallel for schedule(static) firstprivate(a, b)
  for (int i = 0; i < N; i += 16) {
    ValueAVX val = ValueAVXLoad(a + i);
    ValueAVXStore(b + i, val);
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

  Value *a = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  Value *b = (Value *)aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));

  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  for (long long i = 0; i < N; i++) {
    b[i] = 1;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(b, N);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value c = foo(a, b, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  for (int i = 0; i < N; i++) {
    expected += a[i];
    computed += b[i];
  }
  printf("Ret = %d, Expected = %d.\n", computed, expected);
  if (computed != expected) {
    gf_panic();
  }
#endif

  return 0;
}
