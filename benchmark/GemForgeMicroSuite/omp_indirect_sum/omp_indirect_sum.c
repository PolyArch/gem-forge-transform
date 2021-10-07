#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>

#include "omp.h"

// Simple indirect sum
typedef int64_t Value;
typedef int64_t IndT;

#define STRIDE 1
// #define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, IndT *ia, int N) {
  Value sum = 0;
#pragma omp parallel for schedule(static) firstprivate(ia, a) reduction(+ : sum)
  for (IndT i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 16 * 1024 * 1024 / sizeof(Value);
const int NN = N;

#include "linear.int64_t.16MB.c"
#include "linear_shuffled.int64_t.16MB.c"

Value *a = linear_16MB;
Value *ia = linear_shuffled_16MB;

// Value a[NN];
// IndT ia[N];

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  // Initialize the index array.
  // for (int i = 0; i < N; ++i) {
  //   // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
  //   ia[i] = i * (NN / N);
  // }

  // // Initialize the data array.
  // for (int i = 0; i < NN; ++i) {
  //   a[i] = i;
  // }

  // // Shuffle it.
  // for (int j = N - 1; j > 0; --j) {
  //   int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
  //   IndT tmp = ia[i];
  //   ia[i] = ia[j];
  //   ia[j] = tmp;
  // }

  gf_detail_sim_start();
  volatile Value ret;
#ifdef WARM_CACHE
  // First warm up it.
  WARM_UP_ARRAY(a, NN);
  WARM_UP_ARRAY(ia, N);
// Initialize the threads.
#pragma omp parallel for schedule(static)
  for (int i = 0; i < numThreads; ++i) {
    __attribute__((unused)) volatile Value v = a[i];
  }
  gf_reset_stats();
#endif
  ret = foo(a, ia, N);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = foo_warm(a, ia, N);
  printf("Ret = %ld, Expected = %ld.\n", ret, expected);
  if (expected != ret) {
    gf_panic();
  }
#endif
  exit(0);

  return 0;
}