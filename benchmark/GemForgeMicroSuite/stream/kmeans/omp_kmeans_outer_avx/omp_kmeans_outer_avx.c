
/**
 * Special implementation using outer-product.
 *
 * Mainly used for PUM.
 */

#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NO_OPENMP
#include <omp.h>
#endif

#include "immintrin.h"

typedef ValueT Value;
typedef int Index;
const int ValueVecLen = 16;
typedef struct {
  float vs[ValueVecLen];
} ValueVec;

/**
 * Parameters:
 *
 * SPLIT_MIN_DIST_CENTER: Avoid the scatter for CPU baseline.
 * IS_PUM: Avoid vectorize the matrix outer-product loop.
 * TRANSPOSE_MATRIX: Transpose features/centers so that inner dim is longer.
 */
#ifndef STATIC_CHUNK_SIZE
#define STATIC_CHUNK_SIZE 0
#endif

#ifndef OFFSET_BYTES
#define OFFSET_BYTES 0
#endif

struct MinCenter {
  union {
    struct {
      Value distance;
      Index center;
    };
    int64_t raw;
  };
};

__attribute__((noinline)) Value computeDist(Value *restrict A, // [M][K]
                                            Value *restrict B, // [K][N]
#ifdef SPLIT_MIN_DIST_CENTER
                                            Value *restrict C, // [M][N]
#else
                                            struct MinCenter
                                                *restrict C, // [M][N]
#endif

                                            int64_t M, int64_t K, int64_t N) {

  __builtin_assume(K >= 16);
  __builtin_assume(K % 16 == 0);
  __builtin_assume(N >= 16);
  __builtin_assume(N % 16 == 0);

  for (int64_t k = 0; k < K; ++k) {

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static) firstprivate(A, B, C, M, K, N, k)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t m = 0; m < M; ++m) {

#pragma ss stream_name "gfm.kmeans.A.ld"
      Value a = A[m * K + k];

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t n = 0; n < N; ++n) {

#pragma ss stream_name "gfm.kmeans.B.ld"
        Value b = B[k * N + n];

        Value diff = (a - b);
        Value diff2 = diff * diff;

#ifdef SPLIT_MIN_DIST_CENTER
        C[m * N + n] += diff2;
#else
        C[m * N + n].distance += diff2;
#endif
      }
    }
  }
  return 0;
}

__attribute__((noinline)) Value transpose_row(const Value *restrict a,
                                              Value *restrict at, int64_t M,
                                              int64_t N) {

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t i = 0; i < M; ++i) {
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = 0; j < N; ++j) {
      at[j * M + i] = a[i * N + j];
    }
  }

  return 0;
}

__attribute__((noinline)) Value findMinCenter(
#ifdef SPLIT_MIN_DIST_CENTER
    Value *distances, // [nPoints][nCenters]
#endif
    struct MinCenter *minCenters, // [nPoints][nCenters]
    int64_t nPoints, int64_t nDims, int64_t nCenters) {

  // Initial for reduction
  struct MinCenter initMinCenter = {.center = 0, .distance = 1e8};
  const int64_t initMinCenterV = *(int64_t *)(&initMinCenter);

#ifndef NO_OPENMP
#ifdef SPLIT_MIN_DIST_CENTER
#pragma omp parallel for schedule(static) firstprivate(                        \
    distances, minCenters, nPoints, nDims, nCenters, initMinCenterV)
#else
#pragma omp parallel for schedule(static)                                      \
    firstprivate(minCenters, nPoints, nDims, nCenters, initMinCenterV)
#endif
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t point = 0; point < nPoints; ++point) {

    __builtin_assume(nCenters >= 16);
    __builtin_assume(nCenters % 16 == 0);

    int64_t curMinCenter = initMinCenterV;

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t center = 0; center < nCenters; ++center) {

      // Update the distance and center for this point.
      // Crazy bit operation to force LLVM generating one Load/Store for this.
#ifdef SPLIT_MIN_DIST_CENTER

      Value dist = distances[point * nCenters + center];
      int64_t raw = (*(int32_t *)&dist) | (center << 32);

#else

      int64_t *pm = (int64_t *)(minCenters + point * nCenters + center);

      int64_t raw = pm[0];
      int32_t x = raw & 0xffffffff;
      float dist = *(Value *)(&x);

#endif

      int32_t y = curMinCenter & 0xffffffff;
      float curMinDist = *(Value *)(&y);

      int smaller = dist < curMinDist;
      curMinCenter = smaller ? raw : curMinCenter;
    }

    int64_t *po = (int64_t *)(minCenters + point * nCenters);
    *po = curMinCenter;
  }

  return 0;
}

__attribute__((noinline)) Value
accCenter(Value *restrict features,               // [nPoints][nDims]
          struct MinCenter *restrict memberships, // [nPoints][nDims]
          Value *restrict newCenters,             // [nThreads][nCenters][nDims]
          Index *restrict clusterSize,            // [nThreads][nCenters]
          int64_t nPoints, int64_t nDims, int64_t nCenters, int64_t nThreads) {

  __builtin_assume(nCenters > 1);
  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  const int64_t nPointsPerThread = nPoints / nThreads;

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(features, memberships, newCenters, clusterSize, nPoints,      \
                 nDims, nCenters, nThreads, nPointsPerThread)
#endif
  for (int64_t thread = 0; thread < nThreads; ++thread) {

    for (int64_t p = 0; p < nPointsPerThread; ++p) {

      const int64_t point = thread * nPointsPerThread + p;

#pragma ss stream_name "gfm.kmeans.update.minCenter.ld"
      Index minCenter = memberships[point * nDims].center;

#pragma ss stream_name "gfm.kmeans.update.clusterSize.ld"
      Index count = clusterSize[thread * nCenters + minCenter];

#pragma ss stream_name "gfm.kmeans.update.clusterSize.st"
      clusterSize[thread * nCenters + minCenter] = count + 1;

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t dim = 0; dim < nDims; ++dim) {
        /**
         * TODO: We need to implement this IndirectStream on OuterLoop.
         */
        Value v = features[point * nDims + dim];
        newCenters[thread * nCenters * nDims + minCenter * nDims + dim] += v;
      }
    }
  }

  return 0;
}

__attribute__((noinline)) Value
rdcCenter(Value *restrict newCenters,     // [nThreads][nCenters][nDims]
          Value *restrict reducedCenters, // [nThreads][nCenters][nDims]
          Index *restrict clusterSize,    // [nThreads][nCenters]
          int64_t nDims, int64_t nCenters, int64_t nThreads) {

  /**
   * ! reducedCenters is just the newCenters.
   * We use a separate "restrict" pointer here so that the compiler can be happy
   * to assume alias free and vectorize the inner loop.
   *
   * Also, we don't bother using atomic operations as after split into strands
   * in LLC, streams implicitly atomic on the cache lines.
   */

  __builtin_assume(nCenters > 1);
  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Reduce the partial result.
  for (int64_t thread = 1; thread < nThreads; ++thread) {
    for (int64_t center = 0; center < nCenters; ++center) {

      // Accumulate the cluster size.
      Index countPartial = clusterSize[thread * nCenters + center];
      Index countAcc = clusterSize[center];
      clusterSize[center] = countAcc + countPartial;

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t dim = 0; dim < nDims; ++dim) {
        Value v = newCenters[thread * nCenters * nDims + center * nDims + dim];
        reducedCenters[center * nDims + dim] += v;
      }
    }
  }

  return 0;
}

__attribute__((noinline)) Value
normCenter(Value *restrict newCenters,  // [nThreads][nCenters][nDims]
           Index *restrict clusterSize, // [nThreads][nCenters]
           int64_t nDims, int64_t nCenters, int64_t nThreads) {

  __builtin_assume(nCenters > 1);
  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Normalize the final result.
  for (int64_t center = 0; center < nCenters; ++center) {
    Index count = clusterSize[center];
    Value div = (count == 0) ? 1 : count;

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t dim = 0; dim < nDims; ++dim) {
      newCenters[center * nDims + dim] /= div;
    }
  }

  return 0;
}

__attribute__((noinline)) Value
driver(Value *restrict features, // [nPoints][nDims]
       Value *restrict centers,  // [nDims][nCenters]
       Value *restrict centersT, // [nCenters][nDims]
#ifdef SPLIT_MIN_DIST_CENTER
       Value *restrict distances,  // [nPoints][nCenters]
       Value *restrict distancesT, // [nCenters][nPoints]
#endif
       struct MinCenter *restrict memberships, // [nPoints][nCenters]
       Value *restrict newCenters,             // [nThreads][nCenters][nDims]
       Index *restrict clusterSize,            // [nThreads][nCenters]
       int64_t nPoints, int64_t nDims, int64_t nCenters, int64_t nThreads) {

  /**
   * For now, if we are going to enable TRANSPOSE_MATRIX:
   *
   * We assume we have both normal/transposed features prepared.
   * And charge these overheads:
   * 1. Transpose centers before computeDist.
   * 2. Transpose distances back after computeDist.
   * 3. NOTE: Transpose only works with SPLIT_MIN_DIST_CENTER.
   *
   */

#ifdef TRANSPOSE_MATRIX

#ifndef SPLIT_MIN_DIST_CENTER
#error "TRANSPOSE_MATRIX only works with SPLIT_MIN_DIST_CENTER"
#endif

  gf_work_begin(5);
  transpose_row(centers, centersT, nDims, nCenters);
  gf_work_end(5);

  gf_work_begin(0);
  computeDist(centersT, features, distancesT, nCenters, nDims, nPoints);
  gf_work_end(0);

  gf_work_begin(6);
  transpose_row(distancesT, distances, nCenters, nPoints);
  gf_work_end(6);

#else

  gf_work_begin(0);
#ifdef SPLIT_MIN_DIST_CENTER
  computeDist(features, centers, distances, nPoints, nDims, nCenters);
#else
  computeDist(features, centers, memberships, nPoints, nDims, nCenters);
#endif
  gf_work_end(0);

#endif

  gf_work_begin(1);
#ifdef SPLIT_MIN_DIST_CENTER
  findMinCenter(distances, memberships, nPoints, nDims, nCenters);
#else
  findMinCenter(memberships, nPoints, nDims, nCenters);
#endif
  gf_work_end(1);

  gf_work_begin(2);
  accCenter(features, memberships, newCenters, clusterSize, nPoints, nDims,
            nCenters, nThreads);
  gf_work_end(2);

  gf_work_begin(3);
  rdcCenter(newCenters, newCenters, clusterSize, nDims, nCenters, nThreads);
  gf_work_end(3);

  gf_work_begin(4);
  normCenter(newCenters, clusterSize, nDims, nCenters, nThreads);
  gf_work_end(4);

  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  int64_t nPoints = 16 * 2048;
  int64_t nDims = 128;
  int64_t nCenters = 8;
  int check = 0;
  int warm = 0;
  int argx = 2;

  assert(sizeof(ValueVec) == sizeof(Value) * ValueVecLen &&
         "Mismatch ValueVec Size.");

  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nPoints = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nDims = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nCenters = atoll(argv[argx - 1]);
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
  uint64_t T = nPoints * nDims;
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB.\n", T * sizeof(Value) / 1024);
  assert(nPoints % numThreads == 0 && "Undivisible Points to Threads.");
  assert(nCenters == nDims && "nCenters != nDims.");

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes =
      T * sizeof(Value)                               // features
      + nPoints * nCenters * sizeof(Value)            // distances
      + nPoints * nCenters * sizeof(Value)            // distancesT
      + nCenters * nDims * sizeof(Value)              // centers
      + nCenters * nDims * sizeof(Value)              // centersT
      + nPoints * nCenters * sizeof(struct MinCenter) // memberships
      + numThreads * nCenters * nDims * sizeof(Value) // newCenters
      + numThreads * nCenters * sizeof(Index)         // clusterSize
      ;

  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *features = buffer;
  Value *distances = features + nPoints * nDims;
  Value *distancesT = distances + nPoints * nCenters;
  Value *centers = distancesT + nPoints * nCenters;
  Value *centersT = centers + nCenters * nDims;
  struct MinCenter *memberships =
      (struct MinCenter *)(centersT + nCenters * nDims);
  Value *newCenters = (Value *)(memberships + nPoints * nCenters);
  Index *clusterSize = (Index *)(newCenters + numThreads * nCenters * nDims);

  // Initialize the array.
  printf("Try to load data.\n");
  if (nPoints == 32768 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nPoints * nDims * sizeof(Value),
                                       (char *)features,
                                       "../kmeans.float.32768.128.data");
  } else if (nPoints == 65536 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nPoints * nDims * sizeof(Value),
                                       (char *)features,
                                       "../kmeans.float.65536.128.data");
  } else if (nPoints == 2048 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nPoints * nDims * sizeof(Value),
                                       (char *)features,
                                       "../kmeans.float.2048.128.data");
  } else if (nPoints == 1024 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nPoints * nDims * sizeof(Value),
                                       (char *)features,
                                       "../kmeans.float.1024.128.data");
  } else {
    assert(0 && "No features.");
  }
  printf("Loaded data.\n");

  // Uniform sample the centers.
  assert(nPoints >= nCenters && "Too few points.");
  assert(nDims >= 16 && "Dims too small.");
  assert((nDims % 16 == 0) && "Dims not multiple of 16.");
  int64_t skip = nPoints / nCenters;
  for (int64_t i = 0; i < nCenters; ++i) {
    for (int64_t j = 0; j < nDims; ++j) {
      // Transpose the center.
      centers[j * nCenters + i] = features[i * skip * nDims + j];
    }
  }

  // Initialize the membership array with index of centers.
  // Randomly assign points to centers.
  for (int64_t i = 0; i < nPoints; ++i) {
    for (int64_t j = 0; j < nCenters; ++j) {
      memberships[i * nCenters + j].center = j;
      memberships[i * nCenters + j].distance = 1e8;
      distances[i * nCenters + j] = 0;
      distancesT[i * nCenters + j] = 0;
    }
  }

  // Clear the some arrays.
  memset(clusterSize, 0, numThreads * nCenters * sizeof(Index));
  memset(newCenters, 0, numThreads * nCenters * nDims * sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.kmeans.features", features, sizeof(features[0]),
                        nDims, nPoints);
  gf_stream_nuca_region("gfm.kmeans.centers", centers, sizeof(centers[0]),
                        nCenters, nDims);
  gf_stream_nuca_region("gfm.kmeans.centersT", centersT, sizeof(centersT[0]),
                        nDims, nCenters);
  gf_stream_nuca_region("gfm.kmeans.memberships", memberships,
                        sizeof(memberships[0]), nCenters, nPoints);
  gf_stream_nuca_region("gfm.kmeans.new_centers", newCenters, sizeof(Value),
                        nDims, nCenters, numThreads);
  gf_stream_nuca_region("gfm.kmeans.cluster_size", clusterSize, sizeof(Index),
                        1, nCenters, numThreads);
  gf_stream_nuca_region("gfm.kmeans.distances", distances, sizeof(distances[0]),
                        nCenters, nPoints);
  gf_stream_nuca_region("gfm.kmeans.distancesT", distancesT,
                        sizeof(distancesT[0]), nPoints, nCenters);
  gf_stream_nuca_align(centers, features, 0);
  gf_stream_nuca_align(memberships, features, 0);
  gf_stream_nuca_align(features, features, 1);
  gf_stream_nuca_align(features, features, nDims);

#ifndef NEW_CENTER_INTERLEAVE
#define NEW_CENTER_INTERLEAVE (nCenters * nDims)
#endif
  gf_stream_nuca_set_property(newCenters,
                              STREAM_NUCA_REGION_PROPERTY_INTERLEAVE,
                              NEW_CENTER_INTERLEAVE);
  /**
   * As a hack, do not initialize memberships for now.
   * TODO: Avoid using extra space for memberships so we remove this.
   */
  gf_stream_nuca_set_property(memberships,
                              STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT, 1);

  // Disable PUM.
  gf_stream_nuca_set_property(newCenters, STREAM_NUCA_REGION_PROPERTY_USE_PUM,
                              0);
  gf_stream_nuca_set_property(clusterSize, STREAM_NUCA_REGION_PROPERTY_USE_PUM,
                              0);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.kmeans.features", features,
                  nPoints * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.centers", centers,
                  nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.memberships", memberships,
                  nPoints * nCenters * sizeof(struct MinCenter));
    gf_warm_array("gfm.kmeans.new_centers", newCenters,
                  numThreads * nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.cluster_size", clusterSize,
                  numThreads * nCenters * sizeof(Index));
#ifdef SPLIT_MIN_DIST_CENTER
    gf_warm_array("gfm.kmeans.distances", distances,
                  nPoints * nCenters * sizeof(Value));
#endif
#ifdef TRANSPOSE_MATRIX
    gf_warm_array("gfm.kmeans.centersT", centersT,
                  nDims * nCenters * sizeof(Value));
    gf_warm_array("gfm.kmeans.distancesT", distancesT,
                  nCenters * nPoints * sizeof(Value));
#endif
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = features[tid];
  }
#endif

  gf_reset_stats();
  driver(features, centers, centersT,
#ifdef SPLIT_MIN_DIST_CENTER
         distances, distancesT,
#endif
         memberships, newCenters, clusterSize, nPoints, nDims, nCenters,
         numThreads);
  gf_detail_sim_end();

  return 0;
}
