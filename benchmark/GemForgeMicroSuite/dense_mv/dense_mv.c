
/**
 * Dense Matrix-Vector multiply. Used to test multiple level streams.
 */

#include "../gem5_pseudo.h"
#include <stdio.h>
#include <stdlib.h>

typedef long long Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) void foo(Value *a, Value *b, uint64_t width,
                                   uint64_t height, Value *c) {
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (uint64_t i = 0; i < height; ++i) {
    Value value = 0;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
    for (uint64_t j = i; j < width; ++j) {
      const uint64_t a_idx = i * width + j;
      const uint64_t b_idx = j;
      value += a[a_idx] * b[b_idx];
    }
    c[i] = value;
  }
}

#define width 16
#define height 1024

Value a[height][width];
Value b[width];
Value c[height];

int main() {

#ifdef WARM_CACHE
  for (uint64_t i = 0; i < height; ++i) {
    for (uint64_t j = 0; j < width; ++j) {
      a[i][j] = i * j;
      b[j] = j;
    }
  }
#endif

  DETAILED_SIM_START();
  foo(a, b, width, height, c);
  DETAILED_SIM_STOP();

#ifdef CHECK
  int mismatched = 0;
  for (uint64_t i = 0; i < height; ++i) {
    Value value = 0;
    for (uint64_t j = i; j < width; ++j) {
      value += a[i][j] * b[j];
    }
    if (value != c[i]) {
      mismatched++;
    }
  }
  printf("Mismatched %d.\n", mismatched);
#endif

  return 0;
}