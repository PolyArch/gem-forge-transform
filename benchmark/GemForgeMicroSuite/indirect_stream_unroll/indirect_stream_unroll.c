#include <stdio.h>
#include <stdlib.h>

#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

// Simple indirect access.
typedef int Value;
typedef int64_t IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_warm(volatile Value *a, IndT *ia, int N) {
  Value sum = 0.0;
  for (IndT i = 1; i < N - STRIDE; i += STRIDE * 2) {
    // Test coalesced multi-line indirect streams.
    IndT *idxPtr = ia + i;
    IndT idx0 = *idxPtr;
    IndT idx1 = *(idxPtr + STRIDE);
    sum += a[idx0];
    sum += a[idx1];
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
  for (IndT i = 1; i < N - STRIDE; i += STRIDE * 2) {
    // Test coalesced multi-line indirect streams.
    IndT *idxPtr = ia + i;
    IndT idx0 = *idxPtr;
    IndT idx1 = *(idxPtr + STRIDE);
    sum += a[idx0];
    sum += a[idx1];
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

#ifdef GEM_FORGE
  m5_detail_sim_start();
#ifdef WARM_CACHE
  // First warm up it.
  volatile Value warm = foo_warm(a, ia, N);
  m5_reset_stats(0, 0);
#endif
#endif

  volatile Value ret = foo(a, ia, N);

#ifdef GEM_FORGE
  m5_detail_sim_end();
#endif

#ifdef CHECK
  Value expected = foo_warm(a, ia, N);
  printf("Ret = %d, Expected = %d.\n", ret, expected);
#endif

  return 0;
}