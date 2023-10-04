#include "gfm_utils.h"

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C, const int64_t M3,
                                   const int64_t N3) {
/*****************************************
 * L3
 *****************************************/
#pragma clang loop unroll(disable)
  for (int64_t n3 = 0; n3 < N3; ++n3) {

#pragma clang loop unroll(disable)
    for (int64_t m3 = 0; m3 < M3; ++m3) {

#pragma clang loop unroll(disable)
      for (int64_t k3 = 0; k3 < 2048; ++k3) {

/*****************************************
 * L3
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
        for (int64_t m2 = 0; m2 < 8; ++m2) {

// Spatial
#pragma clang loop unroll(disable)
          for (int64_t n2 = 0; n2 < 8; ++n2) {

/*****************************************
 * L2
 *****************************************/
#pragma clang loop unroll(disable)
            for (int64_t m1 = 0; m1 < 128; ++m1) {

#pragma clang loop unroll(disable)
              for (int64_t n1 = 0; n1 < 8; ++n1) {

/*****************************************
 * L2
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
                for (int64_t n0 = 0; n0 < 16; ++n0) {

                  int64_t n_idx = n0 + n1 * 16 + n2 * 128 + n3 * 1024;
                  int64_t m_idx = m1 + m2 * 128 + m3 * 1024;
                  int64_t k_idx = k3;
#pragma ss stream_name "A.ld/split-at-loop=3,4"
                  float a_val = A[k_idx + m_idx * 2048];
#pragma ss stream_name "B.ld/split-at-loop=3,4"
                  float b_val = B[n_idx + k_idx * 2048];
                  C[n_idx + m_idx * 2048] += a_val * b_val;
                }
              }
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int numThreads = 1;
  int check = 0;
  int warm = 0;
  int argx = 2;

  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    warm = atoi(argv[argx - 1]);
  }
  argx++;
  printf("Number of Threads: %d.\n", numThreads);

  float *A = alignedAllocAndTouch(4194304, sizeof(float));
  float *B = alignedAllocAndTouch(4194304, sizeof(float));
  float *C = alignedAllocAndTouch(4194304, sizeof(float));
  gf_stream_nuca_region("A", A, sizeof(A[0]), 2048, 2048);
  gf_stream_nuca_region("B", B, sizeof(B[0]), 2048, 2048);
  gf_stream_nuca_region("C", C, sizeof(C[0]), 2048, 2048);
  gf_stream_nuca_remap();
  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("A", A, 4194304 * sizeof(A[0]));
    gf_warm_array("B", B, 4194304 * sizeof(B[0]));
    gf_warm_array("C", C, 4194304 * sizeof(C[0]));
  }
  gf_reset_stats();
  foo(A, B, C, 2, 2);
  gf_detail_sim_end()
}
