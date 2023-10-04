
#define TILE_L 16
#define TILE_M 128
#define TILE_N 16
#define LOOP_ORDER LOOP_LNM_MLN
#include "../../omp_mm_inner_avx/omp_mm_inner_avx.c"
        