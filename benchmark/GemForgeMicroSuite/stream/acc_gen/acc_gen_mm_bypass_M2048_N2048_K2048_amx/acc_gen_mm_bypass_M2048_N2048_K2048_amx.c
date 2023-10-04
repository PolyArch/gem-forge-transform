#include "gfm_utils.h"
#include <omp.h>

const int64_t N = 2048;
const int64_t M = 2048;
const int64_t K = 2048;

__attribute__((noinline)) void foo(int8_t *restrict A, int8_t *restrict B,
                                   int32_t *restrict C, const int64_t M4,
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

              int64_t c_idx = n8 * 256 * M4 * N5 + m11 * 2048 * M4 * N5 +
                              m13 * 2048 * M4 * N5 * M11 +
                              n16 * 8192 * M4 * N5 * M11 +
                              m17 * 65536 * M4 * N5 * M11;

#pragma ss stream_name "C00.ld/split-at-loop=0,1/reuse=L1=8"
              _tile_loadd(0, C + c_idx, 16 * sizeof(C[0]));
#pragma ss stream_name "C01.ld/split-at-loop=0,1/reuse=L1=8"
              _tile_loadd(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "C10.ld/split-at-loop=0,1/reuse=L1=8"
              _tile_loadd(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "C11.ld/split-at-loop=0,1/reuse=L1=8"
              _tile_loadd(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));

#pragma clang loop unroll(disable)
              for (int64_t k7 = 0; k7 < 8; ++k7) {

                int64_t a_idx = k7 * 1024 * M4 + k10 * 8192 * M4 +
                                m11 * 32768 * M4 + m13 * 32768 * M4 * M11 +
                                m17 * 131072 * M4 * M11;

                int64_t b_idx = k7 * 1024 * N5 + n8 * 8192 * N5 +
                                k10 * 65536 * N5 + n16 * 262144 * N5;

#pragma ss stream_name "A0.ld/split-at-loop=0,1/reuse=L1=8"
                _tile_loadd(4, A + a_idx, 64 * sizeof(A[0]));
#pragma ss stream_name "A1.ld/split-at-loop=0,1/reuse=L1=8"
                _tile_loadd(5, A + a_idx + 1024, 64 * sizeof(A[0]));

#pragma ss stream_name "B0.ld/split-at-loop=0,1/bypass=L1"
                _tile_loadd(6, B + b_idx, 64 * sizeof(B[0]));
#pragma ss stream_name "B1.ld/split-at-loop=0,1/bypass=L1"
                _tile_loadd(7, B + b_idx + 1024, 64 * sizeof(B[0]));

                _tile_dpbssd(0, 4, 6);
                _tile_dpbssd(1, 4, 7);
                _tile_dpbssd(2, 5, 6);
                _tile_dpbssd(3, 5, 7);
              }

#pragma ss stream_name "C00.st/split-at-loop=0,1/spatial-pin/no-ld-st-merge"
              _tile_stored(0, C + c_idx, 16 * sizeof(C[0]));
#pragma ss stream_name "C01.st/split-at-loop=0,1/spatial-pin/no-ld-st-merge"
              _tile_stored(1, C + c_idx + 1 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "C10.st/split-at-loop=0,1/spatial-pin/no-ld-st-merge"
              _tile_stored(2, C + c_idx + 2 * 16 * 16, 16 * sizeof(C[0]));
#pragma ss stream_name "C11.st/split-at-loop=0,1/spatial-pin/no-ld-st-merge"
              _tile_stored(3, C + c_idx + 3 * 16 * 16, 16 * sizeof(C[0]));
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

  // omp_set_dynamic(0);
  // omp_set_num_threads(numThreads);
  // omp_set_schedule(omp_sched_static, 0);
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
  foo(A, B, C, 2, 2, 2);
  gf_detail_sim_end()
}
