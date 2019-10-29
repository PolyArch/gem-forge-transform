#include "gem5/m5ops.h"
#include <stdlib.h>

// Simple indirect access.
typedef int Value;

__attribute__((noinline)) Value foo_warm(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += 16) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += 16) {
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

  // Shuffle it.
  for (int j = N - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    int tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  volatile Value ret;
  // First warm up it.
  ret = foo_warm(a, ia, N);
  m5_detail_sim_start();
  ret = foo(a, ia, N);
  m5_detail_sim_end();
  return 0;
}