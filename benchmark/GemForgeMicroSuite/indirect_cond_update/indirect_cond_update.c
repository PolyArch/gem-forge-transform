/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

typedef long long Value;
typedef int64_t IndT;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, IndT *ia,
                                    int N) {
  Value sum = 0.0;
#pragma clang loop vectorize(disable) unroll(disable)
  for (IndT i = 0; i < N; i += STRIDE) {
    IndT iaValue = ia[i];
    if (a[iaValue] & 0x1) {
      b[iaValue] = 1;
    }
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const long long N = 65536 * 2;
Value a[N];
Value b[N];
Value c[N];
IndT ia[N];

int main() {

  for (int i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i;
  }

  // Initialize the data array.
  for (int i = 0; i < N; ++i) {
    a[i] = i;
    b[i] = i;
    c[i] = 0;
  }

  // Shuffle it.
  for (int j = N - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    IndT tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i += 64 / sizeof(IndT)) {
    volatile IndT iaValue = ia[i];
  }
  for (long long i = 0; i < N; i += 64 / sizeof(Value)) {
    volatile Value aV = a[i];
    volatile Value bV = b[i];
  }
#endif
  m5_reset_stats(0, 0);
  Value ret = foo(a, b, c, ia, N);
  m5_detail_sim_end();

#ifdef CHECK
  long long expectedB = N * N / 4;
  long long computedB = 0;
  printf("Start computed.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    computedB += b[i];
  }
  printf("ExpectedB = %lld ComputedB = %lld.\n", expectedB, computedB);
  // printf("ExpectedC = %lld ComputedC = %lld.\n", expectedC, computedC);
  // for (int i = 0; i < N; i += STRIDE) {
  //   printf("i = %d a = %f b = %f c = %f c_x = %f.\n",
  //     i, a[i], b[i], c[i]);
  // }
#endif

  return 0;
}
