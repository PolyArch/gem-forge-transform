
#define TILE_L 4
#define TILE_M 512
#define TILE_N 4
#define LOOP_ORDER LOOP_LNM_MLN
#include "../../omp_mm_inner_avx/omp_mm_inner_avx.c"
        