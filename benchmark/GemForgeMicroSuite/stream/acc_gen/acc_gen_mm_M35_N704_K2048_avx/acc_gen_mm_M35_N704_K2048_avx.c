#include "gfm_utils.h"

const int64_t M = 35;
const int64_t N = 704;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C) {
/*****************************************
 * L3
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
  for (int64_t k3 = 0; k3 < 8; ++k3) {

// Spatial
#pragma clang loop unroll(disable)
    for (int64_t m3 = 0; m3 < 7; ++m3) {

/*****************************************
 * L3
 *****************************************/
#pragma clang loop unroll(disable)
      for (int64_t k2 = 0; k2 < 8; ++k2) {

/*****************************************
 * L2
 *****************************************/
#pragma clang loop unroll(disable)
        for (int64_t m1 = 0; m1 < 5; ++m1) {

#pragma clang loop unroll(disable)
          for (int64_t k1 = 0; k1 < 32; ++k1) {

#pragma clang loop unroll(disable)
            for (int64_t n1 = 0; n1 < 44; ++n1) {

/*****************************************
 * L2
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
              for (int64_t n0 = 0; n0 < 16; ++n0) {

                int64_t m_idx = m1 + m3 * 5;
                int64_t n_idx = n0 + n1 * 16;
                int64_t k_idx = k1 + k2 * 32 + k3 * 256;
#pragma ss stream_name "A.ld/split-at-loop=0,1"
                float a_val = A[k_idx + m_idx * 2048];
#pragma ss stream_name "B.ld/split-at-loop=0,1"
                float b_val = B[n_idx + k_idx * 704];
#pragma ss stream_name "C.ld/split-at-loop=0,1"
                float c_val = C[n_idx + m_idx * 704];
#pragma ss stream_name "C.st/spatial-pin/no-ld-st-merge/split-at-loop=0,1"
                C[n_idx + m_idx * 704] = c_val + a_val * b_val;
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

  float *A = alignedAllocAndTouch(71680, sizeof(float));
  float *B = alignedAllocAndTouch(1441792, sizeof(float));
  float *C = alignedAllocAndTouch(24640, sizeof(float));
  gf_stream_nuca_region("A", A, sizeof(A[0]), 2048, 35);
  gf_stream_nuca_region("B", B, sizeof(B[0]), 704, 2048);
  gf_stream_nuca_region("C", C, sizeof(C[0]), 704, 35);
  gf_stream_nuca_remap();
  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("A", A, 71680 * sizeof(A[0]));
    gf_warm_array("B", B, 1441792 * sizeof(B[0]));
    gf_warm_array("C", C, 24640 * sizeof(C[0]));
  }
  gf_reset_stats();
  foo(A, B, C);
  gf_detail_sim_end()
}
