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

__attribute__((noinline, noloopidiom)) Value
gather(const Value *restrict features,  // [nFeatures][nDims]
       const Index *restrict neighbors, // [nPoints]
       Value *restrict t1,              // [nThreads][nDims][nDims]
       int64_t nDims) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Gather features.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t point = 0; point < nDims; ++point) {

    Index index = neighbors[point];

    /**
     * Manually vectorize it as OpenMP failed to capture the restrict keyword.
     */

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; dim += 16) {
      ValueAVX v = ValueAVXLoad(features + index * nDims + dim);
      ValueAVXStore(t1 + point * nDims + dim, v);
    }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; ++dim) {
      Value v = myFeatures[index * nDims + dim];
      t1[point * nDims + dim] = v;
    }
#endif
  }

  return 0;
}

__attribute__((noinline)) Value *
mlp(const Value *restrict weights, // [nLayers][nDims][nDims]
    Value *restrict t1,            // [nThreads][nDims][nDims]
    Value *restrict t2,            // [nThreads][nDims][nDims]
    int64_t nDims, int64_t layer) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

#ifdef MLP_OUTER // outer-prod version

  for (int64_t k = 0; k < nDims; ++k) {

    for (int64_t row = 0; row < nDims; ++row) {

      Value a = t1[row * nDims + k];

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < nDims; col += 16) {

        ValueAVX vw =
            ValueAVXLoad(weights + layer * nDims * nDims + col * nDims + k);
        ValueAVX vo = ValueAVXLoad(t2 + row * nDims + col);
        ValueAVX va = ValueAVXSet1(a);

        ValueAVX s = ValueAVXAdd(vo, ValueAVXMul(va, vw));

        ValueAVXStore(t2 + row * nDims + col, s);
      }

#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < nDims; ++col) {

        Value w = weights[layer * nDims * nDims + k * nDims + col];
        Value o = t2[row * nDims + col];

        Value s = o + a * w;
        t2[rows * nDims + col] = s;
      }

#endif
    }
  }

  // Apply the relu.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t row = 0; row < nDims; ++row) {

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t col = 0; col < nDims; ++col) {
      Value v = t2[row * nDims + col];
      Value relu = (v > 0) ? v : 0;
      t2[row * nDims + col] = relu;
    }
  }

#else // inner-prod version (weight is transposed)

  for (int64_t row = 0; row < nDims; ++row) {
    for (int64_t col = 0; col < nDims; ++col) {

      Value sum = 0;

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t k = 0; k < nDims; ++k) {
        Value a = t1[row * nDims + k];
        Value w = weights[layer * nDims * nDims + col * nDims + k];
        sum += a * w;
      }

      Value relu = (sum > 0) ? sum : 0;
      t2[row * nDims + col] = relu;
    }
  }

#endif

  return t1;
}

__attribute__((noinline)) Value *
mlp_multi_layer(const Value *restrict weights, // [nLayers][nDims][nDims]
                Value *restrict t1,            // [nThreads][nDims][nDims]
                Value *restrict t2,            // [nThreads][nDims][nDims]
                int64_t nDims, int64_t nLayers) {

  __builtin_assume(nLayers > 0);

  // Apply MLP
  for (int64_t layer = 0; layer < nLayers; ++layer) {

    mlp(weights, t1, t2, nDims, layer);

    // Swap t1 and t2.
    Value *t = t1;
    t1 = t2;
    t2 = t;
  }

  return t1;
}

__attribute__((noinline)) Value
writeback(Value *restrict results, // [nPoints][nDims]
          Value *restrict t1,      // [nThreads][nDims][nDims]
          int64_t nDims) {
  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Write back the result.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t row = 0; row < nDims; ++row) {

    /**
     * Manually vectorize it as OpenMP failed to capture the restrict keyword.
     */

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; dim += 16) {
      ValueAVX v = ValueAVXLoad(t1 + row * nDims + dim);
      ValueAVXStore(results + row * nDims + dim, v);
    }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; ++dim) {
      Value v = t1[row * nDims + dim];
      results[row * nDims + dim] = v;
    }
#endif
  }

  return 0;
}

__attribute__((noinline)) Value
foo(Value *restrict results,         // [nPoints][nDims]
    const Value *restrict features,  // [nFeatures][nDims]
    const Index *restrict neighbors, // [nPoints]
    const Value *restrict weights,   // [nLayers][nDims][nDims]
    Value *restrict buffer1,         // [nThreads][nDims][nDims]
    Value *restrict buffer2,         // [nThreads][nDims][nDims]
    int64_t nPoints, int64_t nDims, int64_t nLayers) {

  assert(nPoints % nDims == 0);
  int64_t nTiles = nPoints / nDims;

#ifndef NO_OPENMP
#pragma omp parallel for firstprivate(results, features, neighbors, weights,   \
                                      buffer1, buffer2, nPoints, nDims,        \
                                      nLayers)
#endif
  for (int64_t tile = 0; tile < nTiles; ++tile) {

#ifndef NO_OPENMP
    int64_t tid = omp_get_thread_num();
#else
    int64_t tid = 0;
#endif

    Value *restrict t1 = buffer1 + tid * nDims * nDims;
    Value *restrict t2 = buffer2 + tid * nDims * nDims;

    gather(features, neighbors + tile * nDims, t1, nDims);
    Value *out = mlp_multi_layer(weights, t1, t2, nDims, nLayers);
    writeback(results + tile * nDims * nDims, out, nDims);
  }

  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  int64_t nPoints = 16 * 2048;
  int64_t nDims = 128;
  int64_t nFeatures = 4 * 1024;
  int64_t nLayers = 3;
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
    nFeatures = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nLayers = atoll(argv[argx - 1]);
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

  uint64_t totalBytes = nPoints * nDims * sizeof(Value)           // results
                        + nFeatures * nDims * sizeof(Value)       // features
                        + nPoints * sizeof(Index)                 // neighbors
                        + nLayers * nDims * nDims * sizeof(Value) // weights
                        + numThreads * nDims * nDims * sizeof(Value) // buffer1
                        + numThreads * nDims * nDims * sizeof(Value) // buffer2
      ;

  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  Value *results = buffer;
  Value *features = (Value *)(results + nPoints * nDims);
  Index *neighbors = (Index *)(features + nFeatures * nDims);
  Value *weights = (Value *)(neighbors + nPoints);
  Value *buffer1 = (Value *)(weights + nLayers * nDims * nDims);
  Value *buffer2 = (Value *)(buffer1 + numThreads * nDims * nDims);

  // Initialize the array.
  printf("Try to load data.\n");
  if (nFeatures == 4096 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nFeatures * nDims * sizeof(Value),
                                       (char *)features,
                                       "../features.float.4096.128.data");
  } else if (nFeatures == 8192 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nFeatures * nDims * sizeof(Value),
                                       (char *)features,
                                       "../features.float.8192.128.data");
  } else {
    assert(0 && "No features.");
  }
  printf("Loaded data.\n");

  // Randomly sample the neighbors.
  printf("Try to initialize neighbors.\n");
  for (int64_t i = 0; i < nPoints; ++i) {
    neighbors[i] = rand() % nFeatures;
  }
  printf("Initialized neighbors.\n");

  // Don't bother initialize weights/buffers.

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.pointnet.results", results, sizeof(Value), nDims,
                        nPoints);
  gf_stream_nuca_region("gfm.pointnet.features", features, sizeof(Value), nDims,
                        nFeatures);
  gf_stream_nuca_region("gfm.pointnet.neighbors", neighbors, sizeof(Index), 1,
                        nPoints);
  gf_stream_nuca_region("gfm.pointnet.weights", weights, sizeof(Value), nDims,
                        nDims, nLayers);
  gf_stream_nuca_region("gfm.pointnet.buffer1", buffer1, sizeof(Value), nDims,
                        nDims, numThreads);
  gf_stream_nuca_region("gfm.pointnet.buffer2", buffer2, sizeof(Value), nDims,
                        nDims, numThreads);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.pointnet.results", results,
                  nPoints * nDims * sizeof(Value));
    gf_warm_array("gfm.pointnet.features", features,
                  nFeatures * nDims * sizeof(Value));
    gf_warm_array("gfm.pointnet.neighbors", neighbors, nPoints * sizeof(Index));
    gf_warm_array("gfm.pointnet.weights", weights,
                  nDims * nDims * nLayers * sizeof(Value));
    gf_warm_array("gfm.pointnet.buffer1", buffer1,
                  nDims * nDims * numThreads * sizeof(Value));
    gf_warm_array("gfm.pointnet.buffer2", buffer2,
                  nDims * nDims * numThreads * sizeof(Value));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = features[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = foo(results, features, neighbors, weights, buffer1,
                                buffer2, nPoints, nDims, nLayers);
  gf_detail_sim_end();

  return 0;
}
