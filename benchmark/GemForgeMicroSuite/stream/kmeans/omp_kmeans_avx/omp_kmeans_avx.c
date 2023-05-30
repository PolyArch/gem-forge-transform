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
typedef int64_t Index;
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

__attribute__((noinline)) Value
foo(Value *features,           // [nPoints][nDims]
    Value *centers,            // [nCenters][nDims]
    Index *memberships,        // [nPoints]
    Value *newCenters,         // [nCenters][nDims]
    Value *newCentersPartial,  // [nThreads][nCenters][nDims]
    Index *clusterSize,        // [nCenters]
    Index *clusterSizePartial, // [nThreads][nCenters]
    int64_t nPoints, int64_t nDims, int64_t nCenters, int64_t nThreads) {

  /**
   * Assume newCenters, newCentersPartial, clusterSize, clusterSizePartial are
   * zero initialized.
   */
  gf_work_begin(0);

#ifndef NO_OPENMP
#pragma omp parallel firstprivate(                                             \
        features, centers, memberships, newCenters, newCentersPartial,         \
            clusterSize, clusterSizePartial, nPoints, nDims, nCenters)
  {

    int64_t tid = omp_get_thread_num();
    Value *myNewCenters = newCentersPartial + tid * nCenters * nDims;
    Index *myClusterSize = clusterSizePartial + tid * nCenters;
#endif

#ifndef NO_OPENMP
#pragma omp for schedule(static) nowait
#endif
    for (int64_t point = 0; point < nPoints; ++point) {

      Value minDist = 1e8;
      int64_t minCenter = 0;

      __builtin_assume(nCenters > 1);
      __builtin_assume(nDims >= 16);
      __builtin_assume(nDims % 16 == 0);

      for (int64_t center = 0; center < nCenters; ++center) {

        Value dist = 0;

#ifndef NO_AVX // Vectorize version.
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

        int smaller = dist < minDist;
        minCenter = smaller ? center : minCenter;
        minDist = smaller ? dist : minDist;
      }

      memberships[point] = minCenter;

      // Update the newCenters.
      myClusterSize[minCenter]++;
    }

#ifndef NO_OPENMP
#pragma omp for schedule(static)
#endif
    for (int64_t point = 0; point < nPoints; ++point) {

      Index minCenter = memberships[point];

      // Manual vectorization to get rid of epilogue.

#ifndef NO_AVX // Vectorize version.

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t dim = 0; dim < nDims; dim += 16) {
        ValueAVX v = ValueAVXLoad(features + point * nDims + dim);
#pragma ss stream_name "gfm.kmeans.new_center.ld"
        ValueAVX s = ValueAVXLoad(myNewCenters + minCenter * nDims + dim);

        ValueAVX w = ValueAVXAdd(v, s);

#pragma ss stream_name "gfm.kmeans.new_center.st"
        ValueAVXStore(myNewCenters + minCenter * nDims + dim, w);
      }

#else // Scalar version.

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; ++dim) {
      Value v = features[point * nDims + dim];
#pragma ss stream_name "gfm.kmeans.new_center.ld"
      Value s = myNewCenters[minCenter * nDims + dim];
#pragma ss stream_name "gfm.kmeans.new_center.st"
      myNewCenters[minCenter * nDims + dim] = s + v;
    }

#endif
    }

#ifndef NO_OPENMP
  }
#endif

  gf_work_end(0);
  gf_work_begin(1);

  // Reduce to the final result after all the parallel region.
  for (int64_t thread = 0; thread < nThreads; ++thread) {

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t i = 0; i < nCenters * nDims; ++i) {
      newCenters[i] += newCentersPartial[thread * nCenters * nDims + i];
    }

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t i = 0; i < nCenters; ++i) {
      clusterSize[i] += clusterSizePartial[thread * nCenters + i];
    }
  }

  gf_work_end(1);
  gf_work_begin(2);

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
  gf_work_end(2);

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

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t featureSize = nPoints * nDims;
  uint64_t centerSize = nCenters * nDims;
  uint64_t membershipSize = nPoints;
  uint64_t centerPartialSize = numThreads * nCenters * nDims;
  uint64_t clusterSizePartialSize = numThreads * nCenters;

  uint64_t totalBytes =
      (featureSize + centerSize + centerSize + centerPartialSize) *
          sizeof(Value) +
      (membershipSize + clusterSizePartialSize + nCenters) * sizeof(Index);
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *features = buffer;
  Index *memberships = (Index *)(features + nPoints * nDims);
  Value *centers = (Value *)(memberships + nPoints);
  Index *clusterSize = (Index *)(centers + nCenters * nDims);
  Index *clusterSizePartial = (Index *)(clusterSize + nCenters);
  Value *newCenters = (Value *)(clusterSizePartial + numThreads * nCenters);
  Value *newCentersPartial = (Value *)(newCenters + nCenters * nDims);

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
  int64_t skip = nPoints / nCenters;
  for (int64_t i = 0; i < nCenters; ++i) {
    for (int64_t j = 0; j < nDims; ++j) {
      centers[i * nDims + j] = features[i * skip * nDims + j];
    }
  }

  // Clear some arrays.
  memset(clusterSize, 0, nCenters * sizeof(Index));
  memset(clusterSizePartial, 0, numThreads * nCenters * sizeof(Index));
  memset(newCenters, 0, nCenters * nDims * sizeof(Value));
  memset(newCentersPartial, 0, numThreads * nCenters * nDims * sizeof(Value));

  // All points assign to the first center.
  for (int64_t i = 0; i < nPoints; ++i) {
    memberships[i] = 0;
  }

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.kmeans.features", features, sizeof(Value), nDims,
                        nPoints);
  gf_stream_nuca_region("gfm.kmeans.memberships", memberships,
                        sizeof(memberships[0]), 1, nPoints);
  gf_stream_nuca_region("gfm.kmeans.centers", centers, sizeof(Value), nDims,
                        nCenters);
  gf_stream_nuca_region("gfm.kmeans.new_centers", newCenters, sizeof(Value),
                        nDims, nCenters);
  gf_stream_nuca_region("gfm.kmeans.new_centers_p", newCentersPartial,
                        sizeof(Value), nDims, nCenters * numThreads);
  gf_stream_nuca_region("gfm.kmeans.cluster_size", clusterSize, sizeof(Index),
                        1, nCenters);
  gf_stream_nuca_region("gfm.kmeans.cluster_size_p", clusterSizePartial,
                        sizeof(Index), 1, nCenters * numThreads);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.kmeans.features", features,
                  nPoints * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.memberships", memberships,
                  nPoints * sizeof(Index));
    gf_warm_array("gfm.kmeans.centers", centers,
                  nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.new_centers", newCenters,
                  nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.new_centers_p", newCentersPartial,
                  numThreads * nCenters * nDims * sizeof(Value));
    gf_warm_array("gfm.kmeans.cluster_size", clusterSize,
                  nCenters * sizeof(Index));
    gf_warm_array("gfm.kmeans.cluster_size_p", clusterSizePartial,
                  numThreads * nCenters * sizeof(Index));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = features[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(
      features, centers, memberships, newCenters, newCentersPartial,
      clusterSize, clusterSizePartial, nPoints, nDims, nCenters, numThreads);
  gf_detail_sim_end();

  return 0;
}
