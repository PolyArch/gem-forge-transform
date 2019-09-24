/**
 * Simple read after write for memory accesses.
 */
#include "../gem5_pseudo.h"
#include <stdio.h>

typedef int Value;

__attribute__((noinline)) Value foo(volatile Value *a, int N) {
// Make sure there is no reuse.
#pragma nounroll
  for (int i = 1; i < N; i += 1) {
    a[i] += a[i - 1];
  }
  return a[N - 1];
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];

int main() {
  for (int i = 0; i < N; ++i) {
    a[i] = 1;
  }
  DETAILED_SIM_START();
  volatile Value ret = foo(a, N);
  DETAILED_SIM_STOP();
  printf("Ret = %d, Expected = %d.\n", ret, N);
  return 0;
}
