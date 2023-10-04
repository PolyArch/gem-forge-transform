#include "gfm_utils.h"
#include <omp.h>

#include "immintrin.h"

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C, const int64_t M3,
                                   const int64_t M1) {

#pragma omp parallel for collapse(2) schedule(static)                          \
    firstprivate(A, B, C, M3, M1)
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
#pragma clang loop unroll(disable)
      for (int64_t m3 = 0; m3 < M3; ++m3) {

#pragma clang loop unroll(disable)
        for (int64_t k3 = 0; k3 < 1; ++k3) {

          /*****************************************
           * L2
           *****************************************/
#pragma clang loop unroll(disable)
          for (int64_t m2 = 0; m2 < 8; ++m2) {

#pragma clang loop unroll(disable)
            for (int64_t k2 = 0; k2 < 4; ++k2) {

              /*****************************************
               * L1
               *****************************************/
#pragma clang loop unroll(disable)
              for (int64_t m1 = 0; m1 < M1; m1 += 1) {
                int64_t m0_idx = (m1 * 8 + 0) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m1_idx = (m1 * 8 + 1) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m2_idx = (m1 * 8 + 2) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m3_idx = (m1 * 8 + 3) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m4_idx = (m1 * 8 + 4) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m5_idx = (m1 * 8 + 5) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m6_idx = (m1 * 8 + 6) + m2 * 16 + m3 * 128 + m4 * 256;
                int64_t m7_idx = (m1 * 8 + 7) + m2 * 16 + m3 * 128 + m4 * 256;

#pragma clang loop unroll(disable)
                for (int64_t n1 = 0; n1 < 8; n1 += 1) {
                  int64_t n0_idx = n1 * 32 + 0 + n4 * 256;
                  int64_t n1_idx = n1 * 32 + 16 + n4 * 256;

#ifdef PACKED_TILE_C
                  const int64_t c_tile_idx =
                      256 * n1 + 2048 * m1 + 2048 * M1 * m2 +
                      2048 * 8 * M1 * m3 + 2048 * 8 * M3 * M1 * n4 +
                      2048 * 8 * 8 * M3 * M1 * m4;
                  ValueAVX c00_val = ValueAVXLoad(C + 16 * 0 + c_tile_idx);
                  ValueAVX c01_val = ValueAVXLoad(C + 16 * 1 + c_tile_idx);
                  ValueAVX c10_val = ValueAVXLoad(C + 16 * 2 + c_tile_idx);
                  ValueAVX c11_val = ValueAVXLoad(C + 16 * 3 + c_tile_idx);
                  ValueAVX c20_val = ValueAVXLoad(C + 16 * 4 + c_tile_idx);
                  ValueAVX c21_val = ValueAVXLoad(C + 16 * 5 + c_tile_idx);
                  ValueAVX c30_val = ValueAVXLoad(C + 16 * 6 + c_tile_idx);
                  ValueAVX c31_val = ValueAVXLoad(C + 16 * 7 + c_tile_idx);
                  ValueAVX c40_val = ValueAVXLoad(C + 16 * 8 + c_tile_idx);
                  ValueAVX c41_val = ValueAVXLoad(C + 16 * 9 + c_tile_idx);
                  ValueAVX c50_val = ValueAVXLoad(C + 16 * 10 + c_tile_idx);
                  ValueAVX c51_val = ValueAVXLoad(C + 16 * 11 + c_tile_idx);
                  ValueAVX c60_val = ValueAVXLoad(C + 16 * 12 + c_tile_idx);
                  ValueAVX c61_val = ValueAVXLoad(C + 16 * 13 + c_tile_idx);
                  ValueAVX c70_val = ValueAVXLoad(C + 16 * 14 + c_tile_idx);
                  ValueAVX c71_val = ValueAVXLoad(C + 16 * 15 + c_tile_idx);
#else
                  ValueAVX c00_val = ValueAVXLoad(C + n0_idx + m0_idx * 2048);
                  ValueAVX c01_val = ValueAVXLoad(C + n1_idx + m0_idx * 2048);
                  ValueAVX c10_val = ValueAVXLoad(C + n0_idx + m1_idx * 2048);
                  ValueAVX c11_val = ValueAVXLoad(C + n1_idx + m1_idx * 2048);

                  ValueAVX c20_val = ValueAVXLoad(C + n0_idx + m2_idx * 2048);
                  ValueAVX c21_val = ValueAVXLoad(C + n1_idx + m2_idx * 2048);
                  ValueAVX c30_val = ValueAVXLoad(C + n0_idx + m3_idx * 2048);
                  ValueAVX c31_val = ValueAVXLoad(C + n1_idx + m3_idx * 2048);

                  ValueAVX c40_val = ValueAVXLoad(C + n0_idx + m4_idx * 2048);
                  ValueAVX c41_val = ValueAVXLoad(C + n1_idx + m4_idx * 2048);
                  ValueAVX c50_val = ValueAVXLoad(C + n0_idx + m5_idx * 2048);
                  ValueAVX c51_val = ValueAVXLoad(C + n1_idx + m5_idx * 2048);

                  ValueAVX c60_val = ValueAVXLoad(C + n0_idx + m6_idx * 2048);
                  ValueAVX c61_val = ValueAVXLoad(C + n1_idx + m6_idx * 2048);
                  ValueAVX c70_val = ValueAVXLoad(C + n0_idx + m7_idx * 2048);
                  ValueAVX c71_val = ValueAVXLoad(C + n1_idx + m7_idx * 2048);
#endif

#pragma clang loop unroll_count(8)
                  for (int64_t k1 = 0; k1 < 16; ++k1) {

                    int64_t k_idx = k1 + k2 * 16 + k3 * 64;

#ifdef PACKED_TILE_B
                    ValueAVX b0_val =
                        ValueAVXLoad(B + 0 + 32 * k1 + 512 * n1 + 4096 * k2 +
                                     16384 * k3 + 16384 * n4);
                    ValueAVX b1_val =
                        ValueAVXLoad(B + 16 + 32 * k1 + 512 * n1 + 4096 * k2 +
                                     16384 * k3 + 16384 * n4);
#else
                    ValueAVX b0_val = ValueAVXLoad(B + n0_idx + k_idx * 2048);
                    ValueAVX b1_val = ValueAVXLoad(B + n1_idx + k_idx * 2048);
#endif

#ifdef PACKED_TILE_A
                    int64_t a_tile_idx =
                        8 * k1 + 8 * 16 * m1 + 8 * 16 * M1 * k2 +
                        8 * 16 * 4 * M1 * m2 + 8 * 16 * 4 * 8 * M1 * k3 +
                        8 * 16 * 4 * 8 * 1 * M1 * m3 +
                        8 * 16 * 4 * 8 * 1 * M1 * M3 * m4;
                    ValueAVX a0_val = ValueAVXSet1(A[a_tile_idx + 0]);
                    ValueAVX a1_val = ValueAVXSet1(A[a_tile_idx + 1]);
#else
                    ValueAVX a0_val = ValueAVXSet1(A[k_idx + m0_idx * 2048]);
                    ValueAVX a1_val = ValueAVXSet1(A[k_idx + m1_idx * 2048]);
#endif

                    c00_val = ValueAVXAdd(c00_val, ValueAVXMul(a0_val, b0_val));
                    c01_val = ValueAVXAdd(c01_val, ValueAVXMul(a0_val, b1_val));
                    c10_val = ValueAVXAdd(c10_val, ValueAVXMul(a1_val, b0_val));
                    c11_val = ValueAVXAdd(c11_val, ValueAVXMul(a1_val, b1_val));

#ifdef PACKED_TILE_A
                    ValueAVX a2_val = ValueAVXSet1(A[a_tile_idx + 2]);
                    ValueAVX a3_val = ValueAVXSet1(A[a_tile_idx + 3]);
#else
                    ValueAVX a2_val = ValueAVXSet1(A[k_idx + m2_idx * 2048]);
                    ValueAVX a3_val = ValueAVXSet1(A[k_idx + m3_idx * 2048]);
#endif

                    c20_val = ValueAVXAdd(c20_val, ValueAVXMul(a2_val, b0_val));
                    c21_val = ValueAVXAdd(c21_val, ValueAVXMul(a2_val, b1_val));
                    c30_val = ValueAVXAdd(c30_val, ValueAVXMul(a3_val, b0_val));
                    c31_val = ValueAVXAdd(c31_val, ValueAVXMul(a3_val, b1_val));

#ifdef PACKED_TILE_A
                    ValueAVX a4_val = ValueAVXSet1(A[a_tile_idx + 4]);
                    ValueAVX a5_val = ValueAVXSet1(A[a_tile_idx + 5]);
#else
                    ValueAVX a4_val = ValueAVXSet1(A[k_idx + m4_idx * 2048]);
                    ValueAVX a5_val = ValueAVXSet1(A[k_idx + m5_idx * 2048]);
#endif

                    c40_val = ValueAVXAdd(c40_val, ValueAVXMul(a4_val, b0_val));
                    c41_val = ValueAVXAdd(c41_val, ValueAVXMul(a4_val, b1_val));
                    c50_val = ValueAVXAdd(c50_val, ValueAVXMul(a5_val, b0_val));
                    c51_val = ValueAVXAdd(c51_val, ValueAVXMul(a5_val, b1_val));

#ifdef PACKED_TILE_A
                    ValueAVX a6_val = ValueAVXSet1(A[a_tile_idx + 6]);
                    ValueAVX a7_val = ValueAVXSet1(A[a_tile_idx + 7]);
#else
                    ValueAVX a6_val = ValueAVXSet1(A[k_idx + m6_idx * 2048]);
                    ValueAVX a7_val = ValueAVXSet1(A[k_idx + m7_idx * 2048]);
#endif

                    c60_val = ValueAVXAdd(c60_val, ValueAVXMul(a6_val, b0_val));
                    c61_val = ValueAVXAdd(c61_val, ValueAVXMul(a6_val, b1_val));
                    c70_val = ValueAVXAdd(c70_val, ValueAVXMul(a7_val, b0_val));
                    c71_val = ValueAVXAdd(c71_val, ValueAVXMul(a7_val, b1_val));
                  }

#ifdef PACKED_TILE_C
                  ValueAVXStore(C + 16 * 0 + c_tile_idx, c00_val);
                  ValueAVXStore(C + 16 * 1 + c_tile_idx, c01_val);
                  ValueAVXStore(C + 16 * 2 + c_tile_idx, c10_val);
                  ValueAVXStore(C + 16 * 3 + c_tile_idx, c11_val);
                  ValueAVXStore(C + 16 * 4 + c_tile_idx, c20_val);
                  ValueAVXStore(C + 16 * 5 + c_tile_idx, c21_val);
                  ValueAVXStore(C + 16 * 6 + c_tile_idx, c30_val);
                  ValueAVXStore(C + 16 * 7 + c_tile_idx, c31_val);
                  ValueAVXStore(C + 16 * 8 + c_tile_idx, c40_val);
                  ValueAVXStore(C + 16 * 9 + c_tile_idx, c41_val);
                  ValueAVXStore(C + 16 * 10 + c_tile_idx, c50_val);
                  ValueAVXStore(C + 16 * 11 + c_tile_idx, c51_val);
                  ValueAVXStore(C + 16 * 12 + c_tile_idx, c60_val);
                  ValueAVXStore(C + 16 * 13 + c_tile_idx, c61_val);
                  ValueAVXStore(C + 16 * 14 + c_tile_idx, c70_val);
                  ValueAVXStore(C + 16 * 15 + c_tile_idx, c71_val);
#else
                  ValueAVXStore(C + n0_idx + m0_idx * 2048, c00_val);
                  ValueAVXStore(C + n1_idx + m0_idx * 2048, c01_val);
                  ValueAVXStore(C + n0_idx + m1_idx * 2048, c10_val);
                  ValueAVXStore(C + n1_idx + m1_idx * 2048, c11_val);
                  ValueAVXStore(C + n0_idx + m2_idx * 2048, c20_val);
                  ValueAVXStore(C + n1_idx + m2_idx * 2048, c21_val);
                  ValueAVXStore(C + n0_idx + m3_idx * 2048, c30_val);
                  ValueAVXStore(C + n1_idx + m3_idx * 2048, c31_val);
                  ValueAVXStore(C + n0_idx + m4_idx * 2048, c40_val);
                  ValueAVXStore(C + n1_idx + m4_idx * 2048, c41_val);
                  ValueAVXStore(C + n0_idx + m5_idx * 2048, c50_val);
                  ValueAVXStore(C + n1_idx + m5_idx * 2048, c51_val);
                  ValueAVXStore(C + n0_idx + m6_idx * 2048, c60_val);
                  ValueAVXStore(C + n1_idx + m6_idx * 2048, c61_val);
                  ValueAVXStore(C + n0_idx + m7_idx * 2048, c70_val);
                  ValueAVXStore(C + n1_idx + m7_idx * 2048, c71_val);
#endif
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
  foo(A, B, C, 2, 2);
  gf_detail_sim_end()
}
