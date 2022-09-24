
/**
 * Special implementation that swaps point/center loop.
 * "cp" stands for the loop order:
 *    for center
 *      for point
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
 * STATIC_CHUNK_SIZE: OpenMP static scheduling chunk size.
 * OFFSET_BYTES:      Offset between arrays.
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

__attribute__((noinline)) Value
computeDist(Value *features,              // [nPoints][nDims]
            Value *centers,               // [nCenters][nDims]
            struct MinCenter *minCenters, // [nPoints][nDims]
            int64_t nPoints, int64_t nDims, Index center) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(features, centers, memberships, nPoints, nDims, center)
#endif
  for (int64_t point = 0; point < nPoints; ++point) {

    Value dist = 0;

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t dim = 0; dim < nDims; ++dim) {

#pragma ss stream_name "gfm.kmeans.feature.ld"
      Value a = features[point * nDims + dim];

#pragma ss stream_name "gfm.kmeans.center.ld"
      Value b = centers[center * nDims + dim];

      Value diff = a - b;

      dist += diff * diff;
    }

    // Update the distance and center for this point.
    // Crazy bit operation to force LLVM generating one Load/Store for this.
    int64_t *pm = (int64_t *)(minCenters + point * nDims);

    int64_t raw = pm[0];
    int32_t y = raw & 0xffffffff;
    float minDist = *(float *)(&y);

    struct MinCenter n;
    n.center = center;
    n.distance = dist;

    int smaller = dist < minDist;
    int64_t ret = smaller ? n.raw : raw;

    *pm = ret;
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
driver(Value *restrict features,               // [nPoints][nDims]
       Value *restrict centers,                // [nCenters][nDims]
       struct MinCenter *restrict memberships, // [nPoints][nDims]
       Value *restrict newCenters,             // [nThreads][nCenters][nDims]
       Index *restrict clusterSize,            // [nThreads][nCenters]
       int64_t nPoints, int64_t nDims, int64_t nCenters, int64_t nThreads) {

  for (Index center = 0; center < nCenters; ++center) {
    gf_work_begin(0);
    volatile Value computed =
        computeDist(features, centers, memberships, nPoints, nDims, center);
    gf_work_end(0);
  }

  gf_work_begin(1);
  accCenter(features, memberships, newCenters, clusterSize, nPoints, nDims,
            nCenters, nThreads);
  gf_work_end(1);

  gf_work_begin(2);
  rdcCenter(newCenters, newCenters, clusterSize, nDims, nCenters, nThreads);
  gf_work_end(2);

  gf_work_begin(3);
  normCenter(newCenters, clusterSize, nDims, nCenters, nThreads);
  gf_work_end(3);

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

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes =
      T * sizeof(Value)                               // features
      + nCenters * nDims * sizeof(Value)              // centers
      + T * sizeof(struct MinCenter)                  // memberships
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
  Value *centers = features + T;
  struct MinCenter *memberships =
      (struct MinCenter *)(centers + nCenters * nDims);
  Value *newCenters = (Value *)(memberships + T);
  Index *clusterSize = (Index *)(newCenters + numThreads * nCenters * nDims);

  // Initialize the array.
  printf("Try to load data.\n");
  if (nPoints == 32768 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nPoints * nDims * sizeof(Value),
                                       (char *)features,
                                       "../kmeans.float.32768.128.data");
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
      centers[i * nDims + j] = features[i * skip * nDims + j];
    }
  }

  // Randomly assign points to centers.
  for (int64_t i = 0; i < nPoints; ++i) {
    memberships[i * nDims].center = rand() % nCenters;
    memberships[i * nDims].distance = 1e8;
  }

  // Clear the some arrays.
  memset(clusterSize, 0, numThreads * nCenters * sizeof(Index));
  memset(newCenters, 0, numThreads * nCenters * nDims * sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.kmeans.features", features, sizeof(features[0]),
                        nDims, nPoints);
  gf_stream_nuca_region("gfm.kmeans.centers", centers, sizeof(centers[0]),
                        nDims, nCenters);
  gf_stream_nuca_region("gfm.kmeans.memberships", memberships,
                        sizeof(memberships[0]), nDims, nPoints);
  gf_stream_nuca_region("gfm.kmeans.new_centers", newCenters, sizeof(Value),
                        nDims, nCenters, numThreads);
  gf_stream_nuca_region("gfm.kmeans.cluster_size", clusterSize, sizeof(Index),
                        1, nCenters, numThreads);
  gf_stream_nuca_align(centers, features, 0);
  gf_stream_nuca_align(memberships, features, 0);

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
                  nPoints * nDims * sizeof(struct MinCenter));
    gf_warm_array("gfm.kmeans.new_centers", newCenters,
                  numThreads * nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.cluster_size", clusterSize,
                  numThreads * nCenters * sizeof(Index));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = features[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed =
      driver(features, centers, memberships, newCenters, clusterSize, nPoints,
             nDims, nCenters, numThreads);
  gf_detail_sim_end();

  return 0;
}
