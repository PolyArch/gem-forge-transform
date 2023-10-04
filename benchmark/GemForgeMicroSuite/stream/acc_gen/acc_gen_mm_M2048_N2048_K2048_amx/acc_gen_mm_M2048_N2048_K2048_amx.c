#include "gfm_utils.h"

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(float *restrict A, float *restrict B,
                                   float *restrict C, const int64_t M4,
                                   const int64_t N5, const int64_t M11) {
/*****************************************
 * L3
 *****************************************/
// Spatial
#pragma clang loop unroll(disable)
  for (int64_t m17 = 0; m17 < 8; ++m17) {

// Spatial
#pragma clang loop unroll(disable)
    for (int64_t n16 = 0; n16 < 8; ++n16) {

/*****************************************
 * L3
 *****************************************/
#pragma clang loop unroll(disable)
      for (int64_t m13 = 0; m13 < 4; ++m13) {

/*****************************************
 * L2
 *****************************************/
#pragma clang loop unroll(disable)
        for (int64_t m11 = 0; m11 < M11; ++m11) {

#pragma clang loop unroll(disable)
          for (int64_t k10 = 0; k10 < 4; ++k10) {

/*****************************************
 * L1
 *****************************************/
#pragma clang loop unroll(disable)
            for (int64_t n8 = 0; n8 < 8; ++n8) {

#pragma clang loop unroll(disable)
              for (int64_t k7 = 0; k7 < 8; ++k7) {

/*****************************************
 * Tile
 *****************************************/
#pragma clang loop unroll(disable)
                for (int64_t n5 = 0; n5 < N5; ++n5) {

                  int64_t b_idx = n5 * 1024 + k7 * 1024 * N5 + n8 * 8192 * N5 +
                                  k10 * 65536 * N5 + n16 * 262144 * N5;

#pragma ss stream_name "B.ld/split-at-loop=0,1/bypass=L1"
                  _tile_loadd(1, B + b_idx, 1024);

#pragma clang loop unroll(disable)
                  for (int64_t m4 = 0; m4 < M4; ++m4) {

                    int64_t a_idx = m4 * 1024 + k7 * 1024 * M4 +
                                    k10 * 8192 * M4 + m11 * 32768 * M4 +
                                    m13 * 32768 * M4 * M11 +
                                    m17 * 131072 * M4 * M11;

                    int64_t c_idx = m4 * 256 + n5 * 256 * M4 +
                                    n8 * 256 * M4 * N5 + m11 * 2048 * M4 * N5 +
                                    m13 * 2048 * M4 * N5 * M11 +
                                    n16 * 8192 * M4 * N5 * M11 +
                                    m17 * 65536 * M4 * N5 * M11;

#pragma ss stream_name "A.ld/split-at-loop=0,1"
                    _tile_loadd(0, A + a_idx, 1024);

#pragma ss stream_name "C.ld/split-at-loop=0,1"
                    _tile_loadd(2, C + c_idx, 1024);

                    /*****************************************
                     * Tile
                     *****************************************/
                    _tile_dpbssd(2, 0, 1);
#pragma ss stream_name "C.st/split-at-loop=0,1/spatial-pin/no-ld-st-merge"
                    _tile_stored(2, C + c_idx, 1024);
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
  foo(A, B, C, 2, 2, 2);
  gf_detail_sim_end()
}
