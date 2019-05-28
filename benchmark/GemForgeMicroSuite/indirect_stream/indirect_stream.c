#include "../gem5_pseudo.h"
#include <stdlib.h>

// Simple indirect access.
typedef double Value;

__attribute__((noinline)) Value foo_warm(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += 1) {
    sum += a[ia[i]];
  }
  return sum;
}

__attribute__((noinline)) Value foo(volatile Value *a, int *ia, int N) {
  Value sum = 0.0;
#pragma nounroll
  for (int i = 0; i < N; i += 1) {
    sum += a[ia[i]];
  }
  return sum;
}

const int N = 65536 * 1024;
Value a[N];
int ia[N];

int main() {
  // Initialize the index array.
  for (int i = 0; i < N; ++i) {
    ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
  }

  volatile Value ret;
  // First warm up it.
  ret = foo_warm(a, ia, N);
  DETAILED_SIM_START();
  ret = foo(a, ia, N);
  DETAILED_SIM_STOP();
  return 0;
}