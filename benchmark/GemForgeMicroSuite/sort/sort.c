/**
 * Simple array sum.
 */
#include "../gem5_pseudo.h"
#include <stdio.h>
#include <stdlib.h>

typedef long long Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) void foo(Value **pa, int N) {
  Value *a = *pa;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (uint64_t i = 0; i < N; i++) {
    Value localMin = a[i];
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
    for (uint64_t j = i + 1; j < N; j++) {
      if (localMin > a[j]) {
        localMin = a[j];
      }
    }
    a[i] = localMin;
  }
}

// 65536*8 is 512kB.
const int N = 512;
Value a[N];
Value b[N];

int main() {

#ifdef WARM_CACHE
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = rand();
#ifdef CHECK
    b[i] = a[i];
#endif
  }
#endif

  Value *pa = a;
  DETAILED_SIM_START();
  foo(&pa, N);
  DETAILED_SIM_STOP();

#ifdef CHECK
  int mismatched = 0;
  for (uint64_t i = 0; i < N; i++) {
    Value localMin = b[i];
    for (uint64_t j = i + 1; j < N; j++) {
      if (localMin > b[j]) {
        localMin = b[j];
      }
    }
    b[i] = localMin;
    if (a[i] != b[i]) {
      mismatched++;
    }
  }
  printf("Sorted mismatch %d.\n", mismatched);
#endif

  return 0;
}
