/**
 * Kernel of pathfinder.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef int Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

#define MIN(a, b) ((a) <= (b) ? (a) : (b))

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  // Make sure there is no reuse.
#pragma clang loop vectorize(enable)
// #pragma clang loop unroll(disable)
  for (int i = 0; i < N; i += STRIDE) {
    Value min = MIN(a[i], MIN(a[i + 1], a[i + 2]));
    int w = b[i];
    c[i] = w + min;
  }
  return c[0];
}

// 65536*4 is 512kB.
const int N = 1024;
Value a[N + 2];
Value b[N];
Value c[N + 2];
Value c_x[N + 2];

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
    b[i] = i * 2;
    c[i] = 0;
  }
  a[N] = N;
  a[N + 1] = N + 1;
  b[N] = 2 * N;
  b[N + 1] = 2 * (N + 1);
#endif

  m5_detail_sim_start();
  volatile Value ret = foo(a, b, c, N);
  m5_detail_sim_end();

#ifdef CHECK
  printf("Start c_x.\n");
  for (int i = 0; i < N; i += STRIDE) {
    c_x[i] = a[i] + b[i];
  }
  Value expected = 0;
  Value computed = 0;
  printf("Start expected.\n");
  for (int i = 0; i < N; i += STRIDE) {
    expected += c_x[i];
  }
  printf("Start computed.\n");
  for (int i = 0; i < N; i += STRIDE) {
    computed += c[i];
  }
  printf("Ret = %llu, Expected = %llu Real = %d.\n", computed, expected, N * (N - 1) * 3 / 2);
  // for (int i = 0; i < N; i += STRIDE) {
  //   printf("i = %d a = %d b = %d c = %d c_x = %d.\n",
  //     i, a[i], b[i], c[i], c_x[i]);
  // }
#endif

  return 0;
}
