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
gather(Value *restrict results,         // [nPoints][nDims]
       const Value *restrict features,  // [nFeatures][nDims]
       const Index *restrict neighbors, // [nPoints]
       int64_t nPoints, int64_t nDims) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t point = 0; point < nPoints; ++point) {

    Index index = neighbors[point];

    /**
     * Manually vectorize it as OpenMP failed to capture the restrict keyword.
     */

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; dim += 16) {
      ValueAVX v = ValueAVXLoad(features + index * nDims + dim);
      ValueAVXStore(results + point * nDims + dim, v);
    }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; ++dim) {
      Value v = myFeatures[index * nDims + dim];
      results[point * nDims + dim] = v;
    }
#endif
  }

  return 0;
}

__attribute__((noinline)) Value
layer_inner(const Value *restrict input,   // [nPoints][nDims]
            const Value *restrict weights, // [nDims][nDims]
            Value *restrict output,        // [nPoints][nDims]
            int64_t nPoints, int64_t nDims) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Apply one layer of MLP. Inner-Prod version.
  for (int64_t col = 0; col < nDims; ++col) {

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t row = 0; row < nPoints; ++row) {

      Value sum = 0;

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
      for (int64_t k = 0; k < nDims; ++k) {

#pragma ss stream_name "gfm.pointnet.mlp_inner.input.ld"
        Value a = input[row * nDims + k];

        Value w = weights[col * nDims + k];
        sum += a * w;
      }

      Value relu = (sum > 0) ? sum : 0;
      output[row * nDims + col] = relu;
    }
  }

  return 0;
}

__attribute__((noinline)) Value
layer_outer(const Value *restrict input,   // [nPoints][nDims]
            const Value *restrict weights, // [nDims][nDims]
            Value *restrict output,        // [nPoints][nDims]
            int64_t nPoints, int64_t nDims) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

  // Apply one layer of MLP. Inner-Prod version.
  for (int64_t k = 0; k < nDims; ++k) {

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t row = 0; row < nPoints; ++row) {

      Value a = input[row * nDims + k];

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < nDims; col += 16) {

        ValueAVX vw = ValueAVXLoad(weights + k * nDims + col);
        ValueAVX vo = ValueAVXLoad(output + row * nDims + col);

        ValueAVX va = ValueAVXSet1(a);

        ValueAVX s = ValueAVXAdd(vo, ValueAVXMul(va, vw));

        ValueAVXStore(output + row * nDims + col, s);
      }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < nDims; ++col) {

        Value w = weights[k * nDims + col];
        Value o = output[row * nDims + col];

        Value s = o + a * w;
        output[row * nDims + col] = s;
      }
#endif
    }
  }

  // Apply the final relu.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t row = 0; row < nPoints; ++row) {

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t col = 0; col < nDims; ++col) {
      Value v = output[row * nDims + col];
      Value relu = (v > 0) ? v : 0;
      output[row * nDims + col] = relu;
    }
  }

  return 0;
}

__attribute__((noinline)) Value
foo(Value *restrict results,         // [nPoints][nDims]
    const Value *restrict features,  // [nFeatures][nDims]
    const Index *restrict neighbors, // [nPoints]
    const Value *restrict weights,   // [nLayers][nDims][nDims]
    Value *restrict buffer,          // [nPoints][nDims]
    int64_t nPoints, int64_t nDims, int64_t nLayers) {

  // Pick the right buffer to start with so that final value in results.
  Value *restrict t1 = (nLayers & 1) ? buffer : results;
  Value *restrict t2 = (nLayers & 1) ? results : buffer;

// Gather features.
#ifndef NO_GATHER
  gf_work_begin(0);
  gather(t1, features, neighbors, nPoints, nDims);
  gf_work_end(0);
#endif

// MLP.
#ifndef NO_MLP
  for (int64_t layer = 0; layer < nLayers; ++layer) {

    const Value *restrict layerWeights = weights + layer * nDims * nDims;

    gf_work_begin(1);
#ifdef MLP_OUTER
    layer_outer(t1, layerWeights, t2, nPoints, nDims);
#else
    // Inner-Prod assumes weights are transposed.
    layer_inner(t1, layerWeights, t2, nPoints, nDims);
#endif
    gf_work_end(1);

    // Swap t1 and t2.
    Value *t = t1;
    t1 = t2;
    t2 = t;
  }
#endif

  // No need to writeback anything.

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
                        + nPoints * nDims * sizeof(Value)         // temp
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
  Value *temp = (Value *)(weights + nLayers * nDims * nDims);

  // Initialize the array.
  printf("Try to load data.\n");
  if (nFeatures == 4096 && nDims == 128) {
    LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nFeatures * nDims * sizeof(Value),
                                       (char *)features,
                                       "../features.float.4096.128.data");
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
  gf_stream_nuca_region("gfm.pointnet.temp", temp, sizeof(Value), nDims,
                        nPoints);

  gf_stream_nuca_set_property(results, STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT,
                              1);
  gf_stream_nuca_set_property(temp, STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT, 1);
  gf_stream_nuca_set_property(features, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
  gf_stream_nuca_set_property(neighbors, STREAM_NUCA_REGION_PROPERTY_USE_PUM,
                              0);
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
    gf_warm_array("gfm.pointnet.temp", temp, nDims * nPoints * sizeof(Value));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = features[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed =
      foo(results, features, neighbors, weights, temp, nPoints, nDims, nLayers);
  gf_detail_sim_end();

  return 0;
}
