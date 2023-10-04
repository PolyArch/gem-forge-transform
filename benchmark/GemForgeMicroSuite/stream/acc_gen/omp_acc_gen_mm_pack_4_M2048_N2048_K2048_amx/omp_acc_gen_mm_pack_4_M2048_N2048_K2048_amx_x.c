#include "gfm_utils.h"
#include <omp.h>

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(int8_t *restrict A, int8_t *restrict B,
                                   int32_t *restrict C, const int64_t N1,
                                   const int64_t M1, const int64_t M2) {

#pragma omp parallel for collapse(2) schedule(static)                          \
    firstprivate(A, B, C, N1, M1, M2)
/*****************************************
 * L3
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
  for (int64_t m4 = 0; m4 < 8; ++m4) {

// Spatial
#pragma clang loop unroll(disable)
    for (int64_t n4 = 0; n4 < 8; ++n4) {

/*****************************************
 * L3
 *****************************************/
#pragma clang loop unroll(disable)
      for (int64_t m3 = 0; m3 < 4; ++m3) {

/*****************************************
 * L2
 *****************************************/
#pragma clang loop unroll(disable)
        for (int64_t m2 = 0; m2 < M2; ++m2) {

#pragma clang loop unroll(disable)
          for (int64_t k2 = 0; k2 < 4; ++k2) {

#pragma clang loop unroll(disable)
            for (int64_t n2 = 0; n2 < 8; ++n2) {

/*****************************************
 * L1
 *****************************************/
#pragma clang loop unroll(disable)
              for (int64_t m1 = 0; m1 < M1; ++m1) {

#pragma clang loop unroll(disable)
                for (int64_t n1 = 0; n1 < N1; ++n1) {

                  // 2x2 tiles for C.
                  int64_t c_idx =
                      4 * 16 * 16 *
                      (n1 +
                       N1 *
                           (m1 +
                            M1 * (n2 +
                                  8 * (m2 + M2 * (m3 + 4 * (n4 + 8 * (m4)))))));

#pragma ss stream_name "gfm.mm.C00.ld"
                  _tile_loadd(0, C + c_idx + 0 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C01.ld"
                  _tile_loadd(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C10.ld"
                  _tile_loadd(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C11.ld"
                  _tile_loadd(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));

#pragma clang loop unroll(disable)
                  for (int64_t k1 = 0; k1 < 8; ++k1) {

                    // It can handle 16x16x64 matrix multiplication for int8

                    int64_t a_idx =
                        2 * 16 * 64 *
                        (k1 +
                         8 * (m1 +
                              M1 * (k2 + 4 * (m2 + M2 * (m3 + 4 * (m4))))));

#pragma ss stream_name "gfm.mm.A0.ld"
                    _tile_loadd(4, A + a_idx + 0 * 16 * 64, 64 * sizeof(A[0]));
#pragma ss stream_name "gfm.mm.A1.ld"
                    _tile_loadd(5, A + a_idx + 1 * 16 * 64, 64 * sizeof(A[0]));

                    int64_t b_idx =
                        2 * 16 * 64 *
                        (k1 + 8 * (n1 + N1 * (n2 + 8 * (k2 + 4 * (n4)))));

#pragma ss stream_name "gfm.mm.B0.ld"
                    _tile_loadd(6, B + b_idx + 0 * 16 * 64, 64 * sizeof(B[0]));
#pragma ss stream_name "gfm.mm.B1.ld"
                    _tile_loadd(7, B + b_idx + 1 * 16 * 64, 64 * sizeof(B[0]));

                    _tile_dpbssd(0, 4, 6);
                    _tile_dpbssd(1, 4, 7);
                    _tile_dpbssd(2, 5, 6);
                    _tile_dpbssd(3, 5, 7);
                  }

#pragma ss stream_name "gfm.mm.C00.st"
                  _tile_stored(0, C + c_idx + 0 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C01.st"
                  _tile_stored(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C10.st"
                  _tile_stored(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C11.st"
                  _tile_stored(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));
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
  int8_t *A = alignedAllocAndTouch(4194304, sizeof(int8_t));
  int8_t *B = alignedAllocAndTouch(4194304, sizeof(int8_t));
  int32_t *C = alignedAllocAndTouch(4194304, sizeof(int32_t));
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
  foo(A, B, C, 1, 1, 2);
  gf_detail_sim_end()
}
