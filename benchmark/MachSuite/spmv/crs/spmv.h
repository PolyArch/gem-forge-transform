/*
Based on algorithm described here:
http://www.cs.berkeley.edu/~mhoemmen/matrix-seminar/slides/UCB_sparse_tutorial_1.pdf
*/

#include "support.h"
#include <stdio.h>
#include <stdlib.h>

// These constants valid for the IEEE 494 bus interconnect matrix
#define MATRIX_E40R5000
#if defined(MATRIX_494_BUS)
#define NNZ 1666
#define N 494
#define MATRIX_INPUT "494_bus_full.mtx"
#elif defined(MATRIX_1138_BUS)
#define NNZ 2596
#define N 1138
#define MATRIX_INPUT "1138_bus.mtx"
#elif defined(MATRIX_E40R5000)
#define NNZ 553956
#define N 17281
#define MATRIX_INPUT "e40r5000.mtx"
#endif

#define TYPE double

void spmv(TYPE val[NNZ], int32_t cols[NNZ], int32_t rowDelimiters[N + 1],
          TYPE vec[N], TYPE out[N]);
////////////////////////////////////////////////////////////////////////////////
// Test harness interface code.

struct bench_args_t {
  TYPE val[NNZ];
  int32_t cols[NNZ];
  int32_t rowDelimiters[N + 1];
  TYPE vec[N];
  TYPE out[N];
};
