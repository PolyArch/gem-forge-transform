
/**
 * Simple correlation matrix. Used to test multi-level loops.
 */

#include "gem5/m5ops.h"
#include <stdio.h>
#include <stdlib.h>

typedef long long Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) void foo(Value *a, Value *b, uint64_t width,
                                   uint64_t height, Value *c) {
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
  for (uint64_t i = 0; i < width; ++i) {
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
    for (uint64_t j = i; j < width; ++j) {
      const uint64_t c_idx = i * width + j;
      c[c_idx] = 0;
#pragma clang loop vectorize(disable)
#pragma clang loop unroll(disable)
      for (uint64_t k = 0; k < height; ++k) {
        const uint64_t a_idx = k * width + i;
        const uint64_t b_idx = k * width + j;
        c[c_idx] += a[a_idx] * b[b_idx];
      }
    }
  }
}

#define width 16
#define height 1024

Value a[height][width];
Value b[height][width];
Value c[width][width];

int main() {

#ifdef WARM_CACHE
  for (uint64_t i = 0; i < height; ++i) {
    for (uint64_t j = 0; j < width; ++j) {
      a[i][j] = i * j;
      b[i][j] = i * j;
    }
  }
#endif

  m5_detail_sim_start();
  foo(a, b, width, height, c);
  m5_detail_sim_end();

#ifdef CHECK
  int mismatched = 0;
  for (uint64_t i = 0; i < width; ++i) {
    for (uint64_t j = i; j < width; ++j) {
      Value expected = 0;
      for (uint64_t k = 0; k < height; ++k) {
        const uint64_t a_idx = k * width + i;
        const uint64_t b_idx = k * width + j;
        expected += a[k][i] * b[k][j];
      }
      if (expected != c[i][j]) {
        mismatched++;
      }
    }
  }
  printf("Mismatched %d.\n", mismatched);
#endif

  return 0;
}