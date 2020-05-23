/**
 * Compute the directional diff between the neighboring elements.
 * Branch implmentation.
 */
#include "gem5/m5ops.h"
#include <stdio.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  Value *a = *pa;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    for (int j = 0; j < N; j += STRIDE) {
      Value v = a[i * N + j];
      int iN = (i == 0) ? 0 : i - 1;
      int iS = (i == N - 1) ? N - 1 : i + 1;
      int jE = (j == N - 1) ? N - 1 : j + 1;
      int jW = (j == 0) ? 0 : j - 1;
      Value dN = a[iN * N + j] - v;
      Value dS = a[iS * N + j] - v;
      Value dE = a[i * N + jE] - v;
      Value dW = a[i * N + jW] - v;
      sum += dN + dS + dE + dW;
    }
  }
  return sum;
}

// This is 1MB
#define N 1024
Value a[N][N];

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    for (long long j = 0; j < N; ++j) {
      a[i][j] = i * N + j;
    }
  }
#endif

  Value *pa = a;
  m5_detail_sim_start();
  volatile Value c = foo(&pa, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  for (int i = 0; i < N; i += STRIDE) {
    for (int j = 0; j < N; j += STRIDE) {
      Value v = a[i][j];
      int iN = (i == 0) ? 0 : i - 1;
      int iS = (i == N - 1) ? N - 1 : i + 1;
      int jE = (j == N - 1) ? N - 1 : j + 1;
      int jW = (j == 0) ? 0 : j - 1;
      Value dN = a[iN][j] - v;
      Value dS = a[iS][j] - v;
      Value dE = a[i][jE] - v;
      Value dW = a[i][jW] - v;
      expected += dN + dS + dE + dW;
    }
  }
  printf("Ret = %llu, Expected = %llu.\n", c, expected);
#endif

  return 0;
}
