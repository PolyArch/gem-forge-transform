
#define TILE_L 32
#define TILE_M 64
#define TILE_N 32
#define LOOP_ORDER LOOP_LNM_MLN
#include "../../omp_mm_inner_avx/omp_mm_inner_avx.c"
        