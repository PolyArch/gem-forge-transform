#define NO_OPENMP
#define IS_PUM
// #define NO_GATHER
#define MLP_OUTER
#define TRANSPOSE_MATRIX
#define TRANSPOSE_MATRIX_ROW
#define TRANSPOSE_MATRIX_REV_COL
#include "../omp_pointnet_avx/omp_pointnet_avx.c"