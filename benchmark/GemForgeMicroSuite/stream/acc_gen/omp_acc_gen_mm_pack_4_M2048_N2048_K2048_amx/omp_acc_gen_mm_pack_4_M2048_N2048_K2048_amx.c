#include "gfm_utils.h"
#include <omp.h>

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C, const int64_t M11,
                                   const int64_t N1, const int64_t K2,
                                   const int64_t M4) {

#pragma omp parallel for collapse(2) schedule(static)                          \
    firstprivate(A, B, C, M11, N1, K2, M4)
/*****************************************
 * L3
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
  for (int64_t m5 = 0; m5 < 8; ++m5) {

// Spatial
#pragma clang loop unroll(disable)
    for (int64_t n5 = 0; n5 < 8; ++n5) {

/*****************************************
 * L3
 *****************************************/
#pragma clang loop unroll(disable)
      for (int64_t m4 = 0; m4 < M4; ++m4) {

/*****************************************
 * L2
 *****************************************/
#pragma clang loop unroll(disable)
        for (int64_t k3 = 0; k3 < 16; ++k3) {

#pragma clang loop unroll(disable)
          for (int64_t n3 = 0; n3 < 8; ++n3) {

/*****************************************
 * L1
 *****************************************/
#pragma clang loop unroll(disable)
            for (int64_t k2 = 0; k2 < K2; ++k2) {

              int64_t b_idx = k2 * 1024 * N1 + n3 * 1024 * N1 * K2 +
                              k3 * 8192 * N1 * K2 + n5 * 131072 * N1 * K2;

#pragma ss stream_name "gfm.mm.B0.ld"
              _tile_loadd(6, B + b_idx + 0 * 16 * 64, 64 * sizeof(B[0]));
#pragma ss stream_name "gfm.mm.B1.ld"
              _tile_loadd(7, B + b_idx + 1 * 16 * 64, 64 * sizeof(B[0]));

#pragma clang loop unroll(disable)
              for (int64_t m2 = 0; m2 < 4; ++m2) {

                // 2x2 tiles for C.
                int64_t c_idx = m2 * 256 * M11 * N1 + n3 * 1024 * M11 * N1 +
                                m4 * 8192 * M11 * N1 +
                                n5 * 8192 * M11 * N1 * M4 +
                                m5 * 65536 * M11 * N1 * M4;

#pragma ss stream_name "gfm.mm.C00.ld"
                _tile_loadd(0, C + c_idx + 0 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C01.ld"
                _tile_loadd(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C10.ld"
                _tile_loadd(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "gfm.mm.C11.ld"
                _tile_loadd(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));

                int64_t a_idx = m2 * 1024 * M11 + k2 * 4096 * M11 +
                                k3 * 4096 * M11 * K2 + m4 * 65536 * M11 * K2 +
                                m5 * 65536 * M11 * K2 * M4;

#pragma ss stream_name "gfm.mm.A0.ld"
                _tile_loadd(4, A + a_idx + 0 * 16 * 64, 64 * sizeof(A[0]));

                _tile_dpbssd(0, 4, 6);

#pragma ss stream_name "gfm.mm.C00.st"
                _tile_stored(0, C + c_idx + 0 * 16 * 16, 16 * sizeof(C[0]));

                _tile_dpbssd(1, 4, 7);

#pragma ss stream_name "gfm.mm.C01.st"
                _tile_stored(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));

#pragma ss stream_name "gfm.mm.A1.ld"
                _tile_loadd(5, A + a_idx + 1 * 16 * 64, 64 * sizeof(A[0]));

                _tile_dpbssd(2, 5, 6);

#pragma ss stream_name "gfm.mm.C10.st"
                _tile_stored(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));

                _tile_dpbssd(3, 5, 7);

#pragma ss stream_name "gfm.mm.C11.st"
                _tile_stored(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));

/*****************************************
 * Tile
 *****************************************/
#pragma clang loop unroll(disable)
                for (int64_t n1 = 0; n1 < N1; ++n1) {

#pragma clang loop unroll(disable)
                  for (int64_t m11 = 0; m11 < M11; ++m11) {

#pragma clang loop unroll(disable)
                    for (int64_t m1 = 0; m1 < 16; ++m1) {

/*****************************************
 * Tile
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
                      for (int64_t n0 = 0; n0 < 16; ++n0) {

// Spatial
#pragma clang loop unroll(disable)
                        for (int64_t k0 = 0; k0 < 64; ++k0) {

                          int64_t a_idx = k0 * 1 + m1 * 64 + m11 * 1024 +
                                          m2 * 1024 * M11 + k2 * 4096 * M11 +
                                          k3 * 4096 * M11 * K2 +
                                          m4 * 65536 * M11 * K2 +
                                          m5 * 65536 * M11 * K2 * M4;

                          int64_t b_idx = k0 * 1 + n0 * 64 + n1 * 1024 +
                                          k2 * 1024 * N1 + n3 * 1024 * N1 * K2 +
                                          k3 * 8192 * N1 * K2 +
                                          n5 * 131072 * N1 * K2;

                          int64_t c_idx =
                              n0 * 1 + m1 * 16 + m11 * 256 + n1 * 256 * M11 +
                              m2 * 256 * M11 * N1 + n3 * 1024 * M11 * N1 +
                              m4 * 8192 * M11 * N1 + n5 * 8192 * M11 * N1 * M4 +
                              m5 * 65536 * M11 * N1 * M4;

#pragma ss stream_name "A.ld/split-at-loop=0,1,10,11"
                          float a_val = A[a_idx];
#pragma ss stream_name "B.ld/split-at-loop=0,1,10,11"
                          float b_val = B[b_idx];
#pragma ss stream_name "C.ld/split-at-loop=0,1,10,11"
                          float c_val = C[c_idx];
#pragma ss stream_name "C.st/spatial-pin/no-ld-st-merge/split-at-loop=0,1,10,11"
                          C[c_idx] = c_val + a_val * b_val;
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
  foo(A, B, C, M11, N1, K2, M4);
  gf_detail_sim_end()
}
