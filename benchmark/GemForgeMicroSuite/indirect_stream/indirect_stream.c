#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

// Simple indirect access.
typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 65536 * 2;
const int NN = 65536 * 16;
// const int NN = N;
Value a[NN];
int ia[N];

int main() {
  // Initialize the index array.
  for (int i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i * (NN / N);
  }

  // Initialize the data array.
  for (int i = 0; i < NN; ++i) {
    a[i] = i;
  }

  // Shuffle it.
  for (int j = N - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    int tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  m5_detail_sim_start();
  volatile Value ret;
#ifdef WARM_CACHE
  // First warm up it.
  ret = foo_warm(a, ia, N);
#endif
  m5_reset_stats(0, 0);
  ret = foo(a, ia, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = foo_warm(a, ia, N);
  printf("Ret = %d, Expected = %d.\n", ret, expected);
#endif

  return 0;
}