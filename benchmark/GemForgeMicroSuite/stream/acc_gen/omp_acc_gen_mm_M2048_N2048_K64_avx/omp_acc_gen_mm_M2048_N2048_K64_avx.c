#include "gfm_utils.h"
#include <omp.h>

#include "immintrin.h"

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C, const int64_t M3) {

#pragma omp parallel for collapse(2) schedule(static) firstprivate(A, B, C, M3)
  /*****************************************
   * L3
   *****************************************/
  // Spatial
  for (int64_t m4 = 0; m4 < 8; ++m4) {

    // Spatial
    for (int64_t n4 = 0; n4 < 8; ++n4) {

      /*****************************************
       * L3
       *****************************************/
      for (int64_t m3 = 0; m3 < 2; ++m3) {

        for (int64_t k3 = 0; k3 < 1; ++k3) {

          /*****************************************
           * L2
           *****************************************/
          for (int64_t m2 = 0; m2 < 8; ++m2) {

            for (int64_t k2 = 0; k2 < 8; ++k2) {

              /*****************************************
               * L1
               *****************************************/
              for (int64_t k1 = 0; k1 < 8; ++k1) {

                int64_t k_idx = k1 + k2 * 8 + k3 * 64;

                for (int64_t m1 = 0; m1 < 16; ++m1) {

                  int64_t m_idx = m1 + m2 * 16 + m3 * 128 + m4 * 256;

                  ValueAVX a_val = ValueAVXSet1(A[k_idx + m_idx * 2048]);

                  for (int64_t n1 = 0; n1 < 16; ++n1) {

                    /*****************************************
                     * L1
                     *****************************************/
                    // Spatial
                    for (int64_t n0 = 0; n0 < 16; n0 += 16) {

                      int64_t n_idx = n0 + n1 * 16 + n4 * 256;
                      ValueAVX b_val = ValueAVXLoad(B + n_idx + k_idx * 2048);
                      ValueAVX c_val = ValueAVXLoad(C + n_idx + m_idx * 2048);
                      ValueAVX s_val =
                          ValueAVXAdd(c_val, ValueAVXMul(a_val, b_val));
                      ValueAVXStore(C + n_idx + m_idx * 2048, s_val);
                    }
                  }
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

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
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
  startThreads(numThreads);
  gf_reset_stats();
  foo(A, B, C, 2);
  gf_detail_sim_end()
}
