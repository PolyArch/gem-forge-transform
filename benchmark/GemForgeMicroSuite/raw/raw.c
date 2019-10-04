/**
 * Simple read after write for memory accesses.
 */
#include "../gem5_pseudo.h"
#include <stdio.h>

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(volatile Value *a, int N) {
// Make sure there is no reuse.
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (int i = 1; i < N; i += 1) {
    a[i] += a[i - 1];
  }
  return a[N - 1];
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];

int main() {

#ifdef WARM_CACHE
  for (int i = 0; i < N; ++i) {
    a[i] = 1;
  }
#endif

  DETAILED_SIM_START();
  volatile Value ret = foo(a, N);
  DETAILED_SIM_STOP();

#ifdef CHECK
  printf("Ret = %d, Expected = %d.\n", ret, N);
#endif

  return 0;
}