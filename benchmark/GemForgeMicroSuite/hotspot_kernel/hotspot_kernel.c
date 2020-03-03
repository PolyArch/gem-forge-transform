/**
 * Simple dense vector add.
 */

#include "gem5/m5ops.h"
#include <stdio.h>

typedef double Value;

#define STRIDE 1
#define CHECK
#define WARM_CACHE

/* chip parameters	*/
const Value t_chip = 0.0005;
const Value chip_height = 0.016;
const Value chip_width = 0.016;
#define MAX_PD (3.0e6)
#define PRECISION 0.001
#define SPEC_HEAT_SI 1.75e6
#define K_SI 100
#define FACTOR_CHIP 0.5
#define OPEN

__attribute__((noinline)) Value foo(Value *restrict a, Value *restrict b,
                                    Value *restrict c, int M, int N) {
  /**
   * Do this to test the floating point inst.
   */
  Value grid_height = chip_height / M;
  Value grid_width = chip_width / N;
  Value Cap = FACTOR_CHIP * SPEC_HEAT_SI * t_chip * grid_width * grid_height;
  Value Rx = grid_width / (2.0 * K_SI * t_chip * grid_height);
  Value Ry = grid_height / (2.0 * K_SI * t_chip * grid_width);
  Value Rz = t_chip / (K_SI * grid_height * grid_width);
  Value max_slope = MAX_PD / (FACTOR_CHIP * t_chip * SPEC_HEAT_SI);
  Value step = PRECISION / max_slope / 1000.0;
  Value Rx_1 = 1.f / Rx;
  Value Ry_1 = 1.f / Ry;
  Value Rz_1 = 1.f / Rz;
  Value Cap_1 = step / Cap;

  N = 1024;

  // Make sure there is no reuse.
  // #pragma clang loop unroll(disable)
  for (uint64_t i = 1; i < M - 1; ++i) {
#pragma clang loop vectorize(enable)
    for (int64_t j = 0; j < N; ++j) {
      uint64_t idx = i * (N) + j;
      uint64_t idxN = idx - (N);
      uint64_t idxS = idx + (N);
      uint64_t idxE = idx + 1;
      uint64_t idxW = idx - 1;
      Value aC = a[idx];
      Value bC = b[idx];
      Value bN = b[idxN];
      Value bS = b[idxS];
      Value bW = b[idxW];
      Value bE = b[idxE];
      Value delta = Cap_1 * (aC + (bS + bN - 2.0f * bC) * Ry_1 +
                             (bW + bE - 2.0f * bC) * Rx_1);
      // Value delta = Cap * (aC +
      //   (bS + bN - 2.0f * bC) * Ry +
      //   (bW + bE - 2.0f * bC) * Rx);
      c[idx] = delta;
    }
  }
  return c[0];
}

// 1024*256*4 is 1MB.
// const int N = 1024;
// const int M = 256;
#define N 1024
#define M 1024
Value a[M][N];
Value b[M][N];
Value c[M][N];
Value d[M][N];

int main() {

  /**
   * Do this to test the floating point inst.
   */
  Value grid_height = chip_height / M;
  Value grid_width = chip_width / N;
  Value Cap = FACTOR_CHIP * SPEC_HEAT_SI * t_chip * grid_width * grid_height;
  Value Rx = grid_width / (2.0 * K_SI * t_chip * grid_height);
  Value Ry = grid_height / (2.0 * K_SI * t_chip * grid_width);
  Value Rz = t_chip / (K_SI * grid_height * grid_width);
  Value max_slope = MAX_PD / (FACTOR_CHIP * t_chip * SPEC_HEAT_SI);
  Value step = PRECISION / max_slope / 1000.0;
  Value Rx_1 = 1.f / Rx;
  Value Ry_1 = 1.f / Ry;
  Value Rz_1 = 1.f / Rz;
  Value Cap_1 = step / Cap;

  m5_detail_sim_start();
#ifdef WARM_CACHE
  // This should warm up the cache.
  for (int i = 0; i < M; ++i) {
    for (int j = 0; j < N; ++j) {
      a[i][j] = i * N + j;
      b[i][j] = i * N + j;
      c[i][j] = 0;
      d[i][j] = 0;
    }
  }
#endif
  m5_reset_stats(0, 0);
  volatile Value ret = foo(&a[0][0], &b[0][0], &c[0][0], M, N);
  m5_detail_sim_end();

#ifdef CHECK
  Value expected = 0;
  Value computed = 0;
  printf("Start expected.\n");
#pragma clang loop vectorize(disable) unroll(disable)
  for (int i = 1; i + 1 < M; ++i) {
#pragma clang loop vectorize(disable) unroll(disable)
    for (int j = 0; j < N; ++j) {
      Value aC = a[i][j];
      Value bC = b[i][j];
      Value bN = b[i - 1][j];
      Value bS = b[i + 1][j];
      Value bW = b[i][j - 1];
      Value bE = b[i][j + 1];
      computed += c[i][j];
      Value delta = Cap_1 * aC;
      // Value delta = Cap * (a[i][j] +
      //   (b[i + 1][j] + b[i - 1][j] - 2.0f * b[i][j]) * Ry +
      //   (b[i][j - 1] + b[i][j + 1] - 2.0f * b[i][j]) * Rx);
      expected += delta;
    }
  }
  printf("Ret = %f, Expected = %f.\n", computed, expected);
  // for (int i = 0; i < M; ++i) {
  //   for (int j = 0; j < N + 2; ++j) {
  //     printf("i %d j %d a %f b %f c %f d %f.\n",
  //       i, j, a[i][j], b[i][j], c[i][j], d[i][j]);
  //   }
  // }
#endif

  return 0;
}
