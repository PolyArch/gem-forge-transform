#define NO_OPENMP
#define NO_AVX
#define PUM_TILE_DIM0_ELEMS 64
#include "../omp_mm_inner_avx/omp_mm_inner_avx.c"