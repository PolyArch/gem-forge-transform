#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

// Simple indirect access.
typedef int Value;
typedef int64_t IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
  for (IndT i = 0; i < N; i += STRIDE) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
// #pragma nounroll
//   for (IndT i = 0; i < N; i += STRIDE) {
//     sum += a[ia[i]];
//   }
#pragma nounroll
  for (IndT i = 0; i < N; i += STRIDE * 4) {
    // Test coalesced multi-line indirect streams.
    IndT idx = i;
    IndT iaValue = ia[idx];
    sum += a[iaValue - 1];
    sum += a[iaValue + 0];
    sum += a[iaValue + 1];
    sum += a[iaValue + 2];
    sum += a[iaValue + 3];
// #pragma nounroll
//     for (IndT j = 0; j < 4; ++j) {
//       // Test short stream.
//       IndT idx = i + j * STRIDE;
//       IndT iaValue = ia[idx];
//       sum += a[iaValue + 0];
//     }
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const int N = 65536 * 2;
const int NN = 65536 * 16;
// const int NN = N;
Value a[NN];
IndT ia[N];

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
    IndT tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  m5_detail_sim_start();
  volatile Value ret;
#ifdef WARM_CACHE
  // First warm up it.
  ret = foo_warm(a, ia, N);
  m5_reset_stats(0, 0);
#endif
  ret = foo(a, ia, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = foo_warm(a, ia, N);
  printf("Ret = %d, Expected = %d.\n", ret, expected);
#endif

  return 0;
}