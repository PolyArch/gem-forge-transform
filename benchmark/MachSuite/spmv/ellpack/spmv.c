/*
Based on algorithm described here:
http://www.cs.berkeley.edu/~mhoemmen/matrix-seminar/slides/UCB_sparse_tutorial_1.pdf
*/

#include "spmv.h"

__attribute__((noinline)) void kernel(TYPE nzval[N * L], int32_t cols[N * L],
                                    TYPE vec[N], TYPE out[N]) {
  int i, j;
  TYPE Si;

  for (int tdg_i = 0; tdg_i < TDG_WORKLOAD_SIZE; ++tdg_i) {
  ellpack_1:
    for (i = 0; i < N; i++) {
      TYPE sum = out[i];
    ellpack_2:
      for (j = 0; j < L; j++) {
        Si = nzval[j + i * L] * vec[cols[j + i * L]];
        sum += Si;
      }
      out[i] = sum;
    }
  }
}

void ellpack(TYPE nzval[N * L], int32_t cols[N * L], TYPE vec[N], TYPE out[N]) {
  for (int tdg_i = 0; tdg_i < TDG_WORKLOAD_SIZE; ++tdg_i) {
    kernel(nzval, cols, vec, out);
  }
}
