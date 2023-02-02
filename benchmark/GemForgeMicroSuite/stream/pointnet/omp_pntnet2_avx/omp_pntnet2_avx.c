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
typedef int32_t Index;
const int ValueVecLen = 16;
typedef struct {
  float vs[ValueVecLen];
} ValueVec;

/**
 * Parameters:
 * NO_OPENMP
 * IS_PUM
 * NO_GATHER
 * MLP_INNER/MLP_OUTER
 * TRANSPOSE_MATRIX
 */
#ifndef STATIC_CHUNK_SIZE
#define STATIC_CHUNK_SIZE 0
#endif

#ifndef OFFSET_BYTES
#define OFFSET_BYTES 0
#endif

inline int64_t max(int64_t a, int64_t b) { return a > b ? a : b; }

struct Coordinate {
  // Use four corrdinates to align to cache line.
  Value x, y, z, w;
};

__attribute__((noinline)) Value
computeDistTo(Value *restrict distances,                     // [nPoints]
              const struct Coordinate *restrict coordinates, // [nPoints]
              const Value x, const Value y, const Value z, int64_t nPoints) {

#ifdef SPLIT_XYZ
  const Value *splitX = (const Value *)(coordinates);
  const Value *splitY = splitX + nPoints;
  const Value *splitZ = splitY + nPoints;
#endif

#ifndef NO_AVX

#ifndef SPLIT_XYZ
#error "AVX should be with split XYZ."
#endif

  assert(nPoints % 16 == 0);

  ValueAVX xAVX = ValueAVXSet1(x);
  ValueAVX yAVX = ValueAVXSet1(y);
  ValueAVX zAVX = ValueAVXSet1(z);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t j = 0; j < nPoints; j += 16) {
    ValueAVX xx = ValueAVXLoad(splitX + j);
    ValueAVX yy = ValueAVXLoad(splitY + j);
    ValueAVX zz = ValueAVXLoad(splitZ + j);

    ValueAVX dx = ValueAVXSub(xx, xAVX);
    ValueAVX dy = ValueAVXSub(yy, yAVX);
    ValueAVX dz = ValueAVXSub(zz, zAVX);

    ValueAVX dx2 = ValueAVXMul(dx, dx);
    ValueAVX dy2 = ValueAVXMul(dy, dy);
    ValueAVX dz2 = ValueAVXMul(dz, dz);

    ValueAVX d = ValueAVXAdd(dx2, ValueAVXAdd(dy2, dz2));

    ValueAVXStore(distances + j, d);
  }

#else // Scalar version

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t j = 0; j < nPoints; ++j) {

#ifdef SPLIT_XYZ
    Value xx = splitX[j];
    Value yy = splitY[j];
    Value zz = splitZ[j];
#else
    Value xx = coordinates[j].x;
    Value yy = coordinates[j].y;
    Value zz = coordinates[j].z;
#endif

    Value dx = xx - x;
    Value dy = yy - y;
    Value dz = zz - z;

    Value d = dx * dx + dy * dy + dz * dz;
    distances[j] = d;
  }
#endif

  return 0;
}

/**
 * Used when reduce over distances to get the furthest point.
 */
struct DistanceIndex {
  union {
    struct {
      Value distance;
      Index index;
    };
    int64_t raw;
  };
};

// Reductor for OpenMP.
inline int64_t reduceFurthestDistanceIndex(int64_t inout, int64_t in) {

  struct DistanceIndex a, b;
  a.raw = inout;
  b.raw = in;

  int further = a.distance > b.distance;
  return further ? a.raw : b.raw;
}

struct DistanceIndexAVX {
  ValueAVX distance;
  __m512i index;
};

inline struct DistanceIndexAVX
reduceFurthestDistanceIndexAVX(struct DistanceIndexAVX inout,
                               struct DistanceIndexAVX in) {

  struct DistanceIndexAVX ret;

  __mmask16 further =
      _mm512_cmp_ps_mask(inout.distance, in.distance, _CMP_GT_OS);

  ret.distance = _mm512_max_ps(inout.distance, in.distance);
  ret.index = _mm512_mask_blend_epi32(further, in.index, inout.index);
  return ret;
}

#pragma omp declare reduction(                                                 \
    reduceFurthestDistanceIndex:int64_t                                        \
    : omp_out = reduceFurthestDistanceIndex(omp_out, omp_in))                  \
    initializer(omp_priv = omp_orig)

#pragma omp declare reduction(                                                 \
    reduceFurthestDistanceIndexAVX                                             \
    : struct DistanceIndexAVX                                                  \
    : omp_out = reduceFurthestDistanceIndexAVX(omp_out, omp_in))               \
    initializer(omp_priv = omp_orig)

__attribute__((noinline)) Index
computeMinDistTo(Value *restrict distances,                     // [nPoints]
                 const struct Coordinate *restrict coordinates, // [nPoints]
                 const Value x, const Value y, const Value z, int64_t nPoints) {

  struct DistanceIndex nextFurthest;
  nextFurthest.distance = 0;
  nextFurthest.index = 0;

  int64_t nextFurthestRaw = nextFurthest.raw;

#ifdef SPLIT_XYZ
  const Value *splitX = (const Value *)(coordinates);
  const Value *splitY = splitX + nPoints;
  const Value *splitZ = splitY + nPoints;
#endif

#ifndef NO_AVX // AVX version.

#ifndef SPLIT_XYZ
#error "AVX should be with split XYZ."
#endif

  assert(nPoints % 16 == 0);

  ValueAVX xAVX = ValueAVXSet1(x);
  ValueAVX yAVX = ValueAVXSet1(y);
  ValueAVX zAVX = ValueAVXSet1(z);

  struct DistanceIndexAVX furthest;
  furthest.index = _mm512_set1_epi32(0);
  furthest.distance = ValueAVXSet1(0);

// #ifndef NO_OPENMP
// #pragma omp parallel for schedule(static, 4096) \
//     reduction(reduceFurthestDistanceIndexAVX \
//               : furthest) firstprivate(nPoints, xAVX, yAVX, zAVX, splitX, \
//                                        splitY, splitZ, distances)
// #else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  // #endif
  for (int64_t j = 0; j < nPoints; j += 16) {

#pragma ss stream_name "gfm.pntnet2.sample.x.ld"
    ValueAVX xx = ValueAVXLoad(splitX + j);
#pragma ss stream_name "gfm.pntnet2.sample.y.ld"
    ValueAVX yy = ValueAVXLoad(splitY + j);
#pragma ss stream_name "gfm.pntnet2.sample.z.ld"
    ValueAVX zz = ValueAVXLoad(splitZ + j);

    ValueAVX dx = ValueAVXSub(xx, xAVX);
    ValueAVX dy = ValueAVXSub(yy, yAVX);
    ValueAVX dz = ValueAVXSub(zz, zAVX);

    ValueAVX dx2 = ValueAVXMul(dx, dx);
    ValueAVX dy2 = ValueAVXMul(dy, dy);
    ValueAVX dz2 = ValueAVXMul(dz, dz);

    ValueAVX d = ValueAVXAdd(dx2, ValueAVXAdd(dy2, dz2));

#pragma ss stream_name "gfm.pntnet2.sample.d.ld"
    ValueAVX dd = ValueAVXLoad(distances + j);

    ValueAVX dmin = ValueAVXMin(d, dd);

    __mmask16 further = _mm512_cmp_ps_mask(dmin, furthest.distance, _CMP_GT_OS);

    furthest.distance = ValueAVXMax(dmin, furthest.distance);

    __m512i jAVX = _mm512_add_epi32(
        _mm512_set_epi32(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
        _mm512_set1_epi32(j));

    furthest.index = _mm512_mask_blend_epi32(further, furthest.index, jAVX);

#pragma ss stream_name "gfm.pntnet2.sample.d.st"
    ValueAVXStore(distances + j, dmin);
  }

  // Horizontally reduce to single value. Don't bother using narrower version.
  { // Treat idx as __m512.
    __m512 dist = furthest.distance;
    __m512 idx = _mm512_castsi512_ps(furthest.index);

    { // 16 to 8.
      __m512 dist0 = dist;
      __m512 dist1 = _mm512_castpd_ps(_mm512_castpd256_pd512(
          _mm512_extractf64x4_pd(_mm512_castps_pd(dist), 1)));

      __m512 idx0 = idx;
      __m512 idx1 = _mm512_castpd_ps(_mm512_castpd256_pd512(
          _mm512_extractf64x4_pd(_mm512_castps_pd(idx), 1)));

      __mmask16 further = _mm512_cmp_ps_mask(dist0, dist1, _CMP_GT_OS);

      dist = _mm512_max_ps(dist0, dist1);
      idx = _mm512_mask_blend_ps(further, idx1, idx0);
    }
    {
      // 8 to 4.
      __m512 dist0 = dist;
      __m512 dist1 = _mm512_castps128_ps512(
          _mm256_extractf128_ps(_mm512_castps512_ps256(dist), 1));

      __m512 idx0 = idx;
      __m512 idx1 = _mm512_castps128_ps512(
          _mm256_extractf128_ps(_mm512_castps512_ps256(idx), 1));

      __mmask16 further = _mm512_cmp_ps_mask(dist0, dist1, _CMP_GT_OS);

      dist = _mm512_max_ps(dist0, dist1);
      idx = _mm512_mask_blend_ps(further, idx1, idx0);
    }
    {
      // 4 to 2.
      __m512 dist0 = dist;
      __m512 dist1 = _mm512_permute_ps(dist, 0xE);

      __m512 idx0 = idx;
      __m512 idx1 = _mm512_permute_ps(idx, 0xE);

      __mmask16 further = _mm512_cmp_ps_mask(dist0, dist1, _CMP_GT_OS);

      dist = _mm512_max_ps(dist0, dist1);
      idx = _mm512_mask_blend_ps(further, idx1, idx0);
    }
    {
      // 2 to 1.
      __m512 dist0 = dist;
      __m512 dist1 = _mm512_permute_ps(dist, 0x2);

      __m512 idx0 = idx;
      __m512 idx1 = _mm512_permute_ps(idx, 0x2);

      __mmask16 further = _mm512_cmp_ps_mask(dist0, dist1, _CMP_GT_OS);

      dist = _mm512_max_ps(dist0, dist1);
      idx = _mm512_mask_blend_ps(further, idx1, idx0);
    }

    nextFurthest.index =
        _mm_extract_epi32(_mm512_castsi512_si128(_mm512_castps_si512(idx)), 0);
    return nextFurthest.index;
  }

#else // Scalar version

#ifndef NO_OPENMP

#ifdef SPLIT_XYZ
#pragma omp parallel firstprivate(nPoints, x, y, z, splitX, splitY, splitZ,    \
                                  distances)
  {
#else
#pragma omp parallel firstprivate(nPoints, x, y, z, coordinates, distances)
  {
#endif

#pragma omp for schedule(static) reduction(reduceFurthestDistanceIndex         \
                                           : nextFurthestRaw)

#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t j = 0; j < nPoints; ++j) {

#ifdef SPLIT_XYZ
      Value xx = splitX[j];
      Value yy = splitY[j];
      Value zz = splitZ[j];
#else
    Value xx = coordinates[j].x;
    Value yy = coordinates[j].y;
    Value zz = coordinates[j].z;
#endif

      Value dx = xx - x;
      Value dy = yy - y;
      Value dz = zz - z;

      Value d = dx * dx + dy * dy + dz * dz;

      Value dd = distances[j];
      Value dmin = d < dd ? d : dd;
      distances[j] = dmin;

      struct DistanceIndex newDistIndex;
      newDistIndex.index = j;
      newDistIndex.distance = dmin;

      int32_t nextFurthestDistRaw = nextFurthestRaw & 0xffffffff;
      Value nextFurthestDist = *(float *)(&nextFurthestDistRaw);

      int further = dmin > nextFurthestDist;
      nextFurthestRaw = further ? newDistIndex.raw : nextFurthestRaw;
    }

#ifndef NO_OPENMP
  }
#endif

#endif

  nextFurthest.raw = nextFurthestRaw;
  return nextFurthest.index;
}

__attribute__((noinline)) Value
furthestSample(Index *restrict centroids,                     // [nCentroids]
               Value *restrict distances,                     // [nPoints]
               const struct Coordinate *restrict coordinates, // [nPoints]
               int64_t nCentroids, int64_t nPoints) {

  // Pick the first point as the initial one.
  centroids[0] = 0;

#ifdef SPLIT_XYZ
  const Value *splitX = (const Value *)(coordinates);
  const Value *splitY = splitX + nPoints;
  const Value *splitZ = splitY + nPoints;
#endif

// We need to set the distance to a large value. No Need for OpenMP.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#ifndef NO_AVX
  for (int64_t i = 0; i < nPoints; i += 16) {
    ValueAVXStore(distances + i, ValueAVXSet1(10000));
  }
#else
  for (int64_t i = 0; i < nPoints; ++i) {
    distances[i] = 10000;
  }
#endif

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t i = 0; i + 1 < nCentroids; ++i) {

    Index curFarthestIdx = centroids[i];

    printf("Sampled %ld/%ld = %d.\n", i, nCentroids, curFarthestIdx);

#ifdef SPLIT_XYZ
    Value x = splitX[curFarthestIdx];
    Value y = splitY[curFarthestIdx];
    Value z = splitZ[curFarthestIdx];
#else
    Value x = coordinates[curFarthestIdx].x;
    Value y = coordinates[curFarthestIdx].y;
    Value z = coordinates[curFarthestIdx].z;
#endif

    centroids[i + 1] =
        computeMinDistTo(distances, coordinates, x, y, z, nPoints);
  }

  return 0;
}

__attribute__((noinline, noloopidiom)) Value
ballQuery(Index *restrict neighbors, // [nCentroids][nSamples]
          Value *restrict distances, // [nThreads][nPoints]
          const struct Coordinate *restrict coordinates, // [nPoints]
          const Index *restrict centroids,               // nCentroids
          int64_t nCentroids, int64_t nSamples, int64_t nPoints, Value radius) {

#ifdef SPLIT_XYZ
  const Value *splitX = (const Value *)(coordinates);
  const Value *splitY = splitX + nPoints;
  const Value *splitZ = splitY + nPoints;
#endif

#ifndef NO_OPENMP
#ifdef SPLIT_XYZ
#pragma omp parallel firstprivate(neighbors, distances, splitX, splitY,        \
                                  splitZ, centroids, nCentroids, nSamples,     \
                                  nPoints, radius)
#else
#pragma omp parallel firstprivate(neighbors, distances, coordinates,           \
                                  centroids, nCentroids, nSamples, nPoints,    \
                                  radius)
#endif
  {
    Value *restrict myDistances = distances + omp_get_thread_num() * nPoints;
#pragma omp for schedule(static)

#else
  Value *restrict myDistances = distances;
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t i = 0; i < nCentroids; ++i) {
      Index centroid = centroids[i];

#ifdef SPLIT_XYZ
      Value x = splitX[centroid];
      Value y = splitY[centroid];
      Value z = splitZ[centroid];
#else
    Value x = coordinates[centroid].x;
    Value y = coordinates[centroid].y;
    Value z = coordinates[centroid].z;
#endif

#ifdef PRINT_PROGRESS
      if (omp_get_thread_num() == 0) {
        printf("BallQuery %ld/%ld.\n", i, nCentroids);
      }
#endif

      computeDistTo(myDistances, coordinates, x, y, z, nPoints);

      // Collect neighbors.
      int64_t samples = 0;
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t j = 0; j < nPoints; ++j) {
        if (myDistances[j] < radius) {
          neighbors[i * nSamples + samples] = j;
          samples++;
          if (samples == nSamples) {
            break;
          }
        }
      }

      // Fill rest neighbors with the first one.
      if (samples < nSamples) {
        Index firstNeighbor = neighbors[i * nSamples];
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
        for (int64_t j = samples; j < nSamples; ++j) {
          neighbors[i * nSamples + j] = firstNeighbor;
        }
      }

      // for (int64_t j = 0; j < nSamples; ++j) {
      //   Index neighbor = neighbors[i * nSamples + j];
      //   if (neighbor < 0 || neighbor >= nPoints) {
      //     printf("SHIT at %ld Centroid %ld j %ld neighbor %ld.\n", i,
      //     centroid, j,
      //            neighbor);
      //     exit(1);
      //   }
      // }
    }
#ifndef NO_OPENMP
  }
#endif

  return 0;
}

__attribute__((noinline, noloopidiom)) Value
gather(Value *restrict results,         // [nPoints][nDims]
       const Value *restrict features,  // [nFeatures][nDims]
       const Index *restrict neighbors, // [nPoints]
       int64_t nPoints, int64_t nDims) {

  __builtin_assume(nDims >= 16);
  __builtin_assume(nDims % 16 == 0);

#ifdef NO_OPENMP
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#else
#pragma omp parallel for schedule(static)                                      \
    firstprivate(nPoints, nDims, neighbors, features, results)
#endif
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

#ifdef FUSE_GATHER_TRANSPOSE
#error "Can not fuse vectorized gather with transpose"
#endif
    }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t dim = 0; dim < nDims; ++dim) {
      Value v = features[index * nDims + dim];

#ifdef FUSE_GATHER_TRANSEPOSE
      results[dim * nPoints + point] = v;
#else
      results[point * nDims + dim] = v;
#endif
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

  // Apply one layer of MLP. Inner-Prod version.
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(input, weights, output, nPoints, nDims)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t row = 0; row < nPoints; ++row) {

    __builtin_assume(nDims >= 16);
    __builtin_assume(nDims % 16 == 0);

#ifdef PRINT_PROGRESS
    if (omp_get_thread_num() == 0) {
      printf("Start row %ld/%ld.\n", row, nPoints);
    }
#endif

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t col = 0; col < nDims; ++col) {

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

__attribute__((noinline)) Value transpose_col(const Value *restrict a,
                                              Value *restrict at, int64_t M,
                                              int64_t N) {

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t j = 0; j < N; ++j) {
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t i = 0; i < M; ++i) {
      at[j * M + i] = a[i * N + j];
    }
  }

  return 0;
}

__attribute__((noinline)) Value layer_outer(const Value *restrict A, // [M][K]
                                            const Value *restrict B, // [K][N]
                                            Value *restrict C,       // [M][N]
                                            int64_t M, int64_t N, int64_t K) {

  __builtin_assume(N >= 16);
  __builtin_assume(N % 16 == 0);
  __builtin_assume(K >= 16);
  __builtin_assume(K % 16 == 0);
  __builtin_assume(M >= 16);
  __builtin_assume(M % 16 == 0);

  // Apply one layer of MLP. Outer-Prod version.
  for (int64_t k = 0; k < K; ++k) {

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t row = 0; row < M; ++row) {

      Value a = A[row * K + k];

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < N; col += 16) {

        ValueAVX vw = ValueAVXLoad(B + k * N + col);
        ValueAVX vo = ValueAVXLoad(C + row * N + col);

        ValueAVX va = ValueAVXSet1(a);

        ValueAVX s = ValueAVXAdd(vo, ValueAVXMul(va, vw));

        ValueAVXStore(C + row * N + col, s);
      }
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t col = 0; col < N; ++col) {

        Value w = B[k * N + col];
        Value o = C[row * N + col];

        Value s = o + a * w;
        C[row * N + col] = s;
      }
#endif
    }
  }

  // Apply the final relu.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int64_t row = 0; row < M; ++row) {

#ifndef NO_AVX // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t col = 0; col < N; ++col) {
      Value v = C[row * N + col];
      Value relu = (v > 0) ? v : 0;
      C[row * N + col] = relu;
    }
  }

  return 0;
}

__attribute__((noinline)) void aggregateMaxFeature(
    Value *restrict out,      // [nCentroids][nOutDims]
    const Value *restrict in, // [nCentroids * nNeighbors][nDims]
    int64_t nOutDims, int64_t nDims, int64_t nCentroids, int64_t nNeighbors) {

#ifdef NO_AGGREGATE
  printf("Skip Aggregate.\n");
  return;
#endif

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(out, in, nOutDims, nDims, nCentroids, nNeighbors)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t i = 0; i < nCentroids; ++i) {

#ifndef NO_AVX
    __builtin_assume(nDims >= 16);
    __builtin_assume(nDims % 16 == 0);
    __builtin_assume(nNeighbors > 0);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = 0; j < nDims; j += 16) {

      ValueAVX max = ValueAVXSet1(0);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t k = 0; k < nNeighbors; ++k) {
        ValueAVX f = ValueAVXLoad(in + i * nNeighbors * nDims + k * nDims + j);
        max = ValueAVXMax(max, f);
      }

      ValueAVXStore(out + i * nOutDims + j, max);
    }

#else

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = 0; j < nDims; ++j) {
      Value max = 0;
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      for (int64_t k = 0; k < nNeighbors; ++k) {
        Value f = in[i * nNeighbors * nDims + k * nDims + j];
        max = f > max ? f : max;
      }
      out[i * nOutDims + j] = max;
    }
#endif
  }
}

__attribute__((noinline)) void
copyFeature(Value *restrict out,      // [nCentroids * nNeighbors][nOutDims]
            const Value *restrict in, // [nCentroids * nNeighbors][nDims]
            int64_t nOutDims, int64_t nDims, int64_t nCentroids,
            int64_t nNeighbors) {

#ifdef NO_AGGREGATE
  printf("Skip CopyFeature.\n");
  return;
#endif

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(out, in, nOutDims, nDims, nCentroids, nNeighbors)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t i = 0; i < nCentroids * nNeighbors; ++i) {

#ifndef NO_AVX
    __builtin_assume(nDims >= 16);
    __builtin_assume(nDims % 16 == 0);
    __builtin_assume(nNeighbors > 0);

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = 0; j < nDims; j += 16) {
      ValueAVX f = ValueAVXLoad(in + i * nDims + j);
      ValueAVXStore(out + i * nOutDims + j, f);
    }

#else

#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
    for (int64_t j = 0; j < nDims; ++j) {
      Value f = in[i * nDims + j];
      out[i * nOutDims + j] = f;
    }
#endif
  }
}

struct SetAbstractorArgs {
  /**
   * We notice that the OutputDim is usually double the InputDim.
   * Hence, the weight for last layer is split into two weights, and we
   * the last layer into two matrix multiplication.
   */
  int64_t nFeatures;
  int64_t nCentroids;
  int64_t nNeighbors;
  Value radius;
  int64_t nLayers;                               // At most 3 layers
  int64_t nDims[4];                              // InputDim, ..., OutputDim
  const Value *restrict features;                // [nFeatures][nDims[0]]
  const struct Coordinate *restrict coordinates; // [nFeatures]
  const Value *restrict weights[4];              // Sum([nDims[i] x nDims[i+1]])
  Value *restrict distances;                     // [nFeatures]
  Index *restrict centroids;                     // [nCentroids]
  Index *restrict neighbors;                     // [nCentroids][nNeighbors]
  // These two works together.
  Value *restrict buffers[4]; // [nCentroids][nNeighbors][nDims[i]]
  Value *restrict outs[4];    // [nCentroids][nNeighbors][nDims[i]]
};

struct PointNet2SSGArgs {
  struct SetAbstractorArgs sa1;
  struct SetAbstractorArgs sa2;
  struct SetAbstractorArgs sa3;
};

__attribute__((noinline)) void
applyOneLayer(Value *restrict t2,
              const Value *restrict weights, // [nDims][nDims]
              const Value *restrict t1,      // [nPoints][nDims]
              int64_t nDims, int64_t nPoints) {

#ifdef NO_MLP
  printf("Skip MLP.\n");
#else

  gf_work_begin(4);

#if defined(MLP_OUTER)

#ifdef TRANSPOSE_MATRIX
  // We assume weight is already transposed.
  layer_outer(weights, t1, t2, nDims, nPoints, nDims);
#else
  layer_outer(t1, weights, t2, nPoints, nDims, nDims);
#endif

#else

#ifdef TRANSPOSE_MATRIX
#error "TRANSPOSE_MATRIX not compatible with MLP_INNER"
#else
  // Inner-Prod assumes weights are transposed.
  layer_inner(t1, weights, t2, nPoints, nDims);
#endif

#endif
  gf_work_end(4);

#endif
}

__attribute__((noinline)) Value foo(struct SetAbstractorArgs args) {

  /************************************************************************
   * Sample centroids.
   ************************************************************************/
  gf_work_begin(0);
#ifdef NO_SAMPLE
  printf("Skip FurthestSample.\n");
#else
  furthestSample(args.centroids, args.distances, args.coordinates,
                 args.nCentroids, args.nFeatures);
#endif
  gf_work_end(0);

  /************************************************************************
   * Ball query neighbors.
   ************************************************************************/
  gf_work_begin(1);
#ifdef NO_BALL_QUERY
  printf("Skip BallQuery.\n");
#else
  ballQuery(args.neighbors, args.distances, args.coordinates, args.centroids,
            args.nCentroids, args.nNeighbors, args.nFeatures, args.radius);
#endif
  gf_work_end(1);

  const int64_t nLayers = args.nLayers;
  const int64_t nPoints = args.nCentroids * args.nNeighbors;
  const int64_t inDim = args.nDims[0];

  /************************************************************************
   * Gather.
   ************************************************************************/
#ifdef NO_GATHER
  printf("Skip Gather.\n");
#else
  gf_work_begin(2);
  gather(args.buffers[0], args.features, args.neighbors, nPoints, inDim);
  gf_work_end(2);
#endif

  /************************************************************************
   * MLP.
   ************************************************************************/

#ifdef TRANSPOSE_MATRIX
  gf_work_begin(3);

#ifdef TRANSPOSE_MATRIX_ROW
  transpose_row(args.buffers[0], args.buffers[1], nPoints, inDim);
#else
  transpose_col(args.buffers[0], args.buffers[1], nPoints, inDim);
#endif

  // Swap the two buffers.
  {
    Value *restrict t0 = args.buffers[0];
    Value *restrict t1 = args.buffers[1];
    args.buffers[0] = t1;
    args.outs[0] = t1;
    args.buffers[1] = t0;
    if (args.outs[1] == t1) {
      args.outs[1] = t0;
    }
  }

  gf_work_end(3);
#endif

  for (int64_t layer = 0; layer < nLayers; ++layer) {

    int64_t inDim = args.nDims[layer];
    int64_t outDim = args.nDims[layer + 1];
    int64_t ratio = outDim / inDim;

    const Value *restrict layerWeights = args.weights[layer];

    Value *restrict buf = args.buffers[layer + 1];
    Value *restrict out = args.outs[layer + 1];

    int needPostProcess = (out != buf);

    for (int i = 0; i < ratio; ++i) {
      printf("Begin Layer %ld/%ld-%d %ldx%ld.\n", layer, nLayers, i, inDim,
             outDim);

      applyOneLayer(args.buffers[layer + 1], layerWeights + i * inDim * inDim,
                    args.outs[layer], inDim, nPoints);

      if (needPostProcess) {
        if (layer + 1 == nLayers) {
          // This is the last layer, need to aggregate the max value.
          aggregateMaxFeature(out + i * inDim, buf, outDim, inDim,
                              args.nCentroids, args.nNeighbors);
        } else {
          // Internal layer, just copy.
          copyFeature(out + i * inDim, buf, outDim, inDim, args.nCentroids,
                      args.nNeighbors);
        }
      }
    }
  }

  // #ifdef TRANSPOSE_MATRIX
  // #error "Transpose is disabled for now"
  //   gf_work_begin(5);

  // #ifdef TRANSPOSE_MATRIX_REV_ROW
  //   transpose_row(t1, t2, nDims, nPoints);
  // #else
  //   transpose_col(t1, t2, nDims, nPoints);
  // #endif

  //   gf_work_end(5);
  // #endif

  // No need to writeback anything.
  return 0;
}

__attribute__((noinline)) Value
fc_layer(const Value *restrict input,   // [N]
         const Value *restrict weights, // [M][N]
         Value *restrict output,        // [M]
         int64_t M, int64_t N) {

  // Apply one FC layer. Inner-Prod version.
#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)                                      \
    firstprivate(input, weights, output, M, N)
#else
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
  for (int64_t row = 0; row < M; ++row) {

    __builtin_assume(N >= 16);
    __builtin_assume(N % 16 == 0);

#ifdef PRINT_PROGRESS
    if (omp_get_thread_num() == 0) {
      printf("Start FC row %ld/%ld.\n", row, nPoints);
    }
#endif

    Value sum = 0;

#if !defined(NO_AVX) && !defined(IS_PUM) // Vectorize version.
#pragma clang loop vectorize_width(16) unroll(disable) interleave(disable)
#else // Scalar version.
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
#endif
    for (int64_t k = 0; k < N; ++k) {

#pragma ss stream_name "gfm.pointnet.mlp_inner.input.ld"
      Value a = input[k];

      Value w = weights[row * N + k];
      sum += a * w;
    }

    Value relu = (sum > 0) ? sum : 0;
    output[row] = relu;
  }

  return 0;
}

void bar(struct SetAbstractorArgs args) {

  assert(args.nLayers == 3);

  for (int i = 0; i < args.nLayers; ++i) {
    fc_layer(args.buffers[i], args.weights[i], args.buffers[i + 1],
             args.nDims[i + 1], args.nDims[i]);
  }

  return;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  int isFC = 0;
  int64_t nFeatures = 4 * 1024;
  int64_t nCentroids = 512;
  int64_t nNeighbors = 32;
  int64_t nLayers = 3;
  const int64_t maxLayers = 3;
  int64_t layerDims[maxLayers + 1];
  float radius = 0.2;
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
    isFC = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nFeatures = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nCentroids = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nNeighbors = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    nLayers = atoll(argv[argx - 1]);
  }
  argx++;
  assert(nLayers >= 1);
  assert(nLayers <= maxLayers);
  for (int i = 0; i < nLayers + 1; ++i) {
    assert(argc >= argx);
    layerDims[i] = atoll(argv[argx - 1]);
    argx++;
  }
  if (argc >= argx) {
    radius = atof(argv[argx - 1]);
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

  int64_t inDims = layerDims[0];
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB.\n",
         nCentroids * nNeighbors * inDims * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  if (isFC) {
    // Different path for FC.
    struct SetAbstractorArgs sa1;
    sa1.nLayers = nLayers;
    sa1.nDims[0] = layerDims[0];
    sa1.nDims[1] = layerDims[1];
    sa1.nDims[2] = layerDims[2];
    sa1.nDims[3] = layerDims[3];

    sa1.buffers[0] = alignedAllocAndTouch(sa1.nDims[0], sizeof(Value));

    for (int i = 0; i < nLayers; ++i) {
      sa1.buffers[i + 1] =
          alignedAllocAndTouch(sa1.nDims[i + 1], sizeof(Value));
      sa1.weights[i] =
          alignedAllocAndTouch(sa1.nDims[i] * sa1.nDims[i + 1], sizeof(Value));
    }

    gf_detail_sim_start();
    if (warm) {
    }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
    for (int tid = 0; tid < numThreads; ++tid) {
      volatile Value x = sa1.buffers[0][tid];
    }
#endif

    gf_reset_stats();
    bar(sa1);
    gf_detail_sim_end();
    return 0;
  }

  Value *features = alignedAllocAndTouch(nFeatures * inDims, sizeof(Value));
  struct Coordinate *coordinates =
      alignedAllocAndTouch(nFeatures, sizeof(struct Coordinate));

  /****************************************************************************
   * The first SetAbstractor (SA)
   * nCentroids = 512
   * radius = 0.2
   * nNeighbors = 32
   * layer0 = 64
   * layer1 = 64
   * layer2 = 128
   *
   * NOTE: When nCentroids * nNeighbors < nDims
   * we allocate extra space for buffer so that PUM is happy to align the data.
   ****************************************************************************/

  struct SetAbstractorArgs sa1;
  sa1.nFeatures = nFeatures;
  sa1.nCentroids = nCentroids;
  sa1.nNeighbors = nNeighbors;
  sa1.radius = radius;
  sa1.nLayers = nLayers;
  sa1.nDims[0] = layerDims[0];
  sa1.nDims[1] = layerDims[1];
  sa1.nDims[2] = layerDims[2];
  sa1.nDims[3] = layerDims[3];
  sa1.features = features;
  sa1.coordinates = coordinates;
  for (int i = 0; i < nLayers; ++i) {
    sa1.weights[i] =
        alignedAllocAndTouch(layerDims[i] * layerDims[i + 1], sizeof(Value));
  }
  // The input buffer.
  sa1.buffers[0] = alignedAllocAndTouch(
      layerDims[0] * max(layerDims[0], nCentroids * nNeighbors), sizeof(Value));
  sa1.outs[0] = sa1.buffers[0];
  for (int i = 1; i < nLayers + 1; ++i) {
    int64_t prevDim = layerDims[i - 1];
    int64_t myDim = layerDims[i];
    // See if we can reuse previous - 1 buffer.
    if (i >= 2 && layerDims[i - 2] == prevDim) {
      sa1.buffers[i] = sa1.buffers[i - 2];
    } else {
      sa1.buffers[i] = alignedAllocAndTouch(
          prevDim * max(prevDim, nCentroids * nNeighbors), sizeof(Value));
    }
    if (i == nLayers) {
      // The last one just have the aggregated results.
      sa1.outs[i] = alignedAllocAndTouch(nCentroids * myDim, sizeof(Value));
    } else {
      if (myDim == prevDim) {
        // That's it.
        sa1.outs[i] = sa1.buffers[i];
      } else {
        // We need to break the mlp into multiple gemm.
        assert(myDim % prevDim == 0);
        assert(myDim > prevDim);
        int64_t ratio = myDim / prevDim;
        sa1.outs[i] = alignedAllocAndTouch(
            myDim * max(myDim, nCentroids * nNeighbors), sizeof(Value));
      }
    }
  }
  sa1.distances = alignedAllocAndTouch(nFeatures * numThreads, sizeof(Value));
  sa1.centroids = alignedAllocAndTouch(nCentroids, sizeof(Index));
  sa1.neighbors = alignedAllocAndTouch(nCentroids * nNeighbors, sizeof(Index));

  // Initialize the array.
  printf("Try to load data nFeatures %ld nDims %ld.\n", nFeatures,
         layerDims[0]);
  char fn[128];
  snprintf(fn, 128, "../features.float.%ld.%ld.data", nFeatures, layerDims[0]);
  LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nFeatures * layerDims[0] * sizeof(Value),
                                     (char *)features, fn);
  snprintf(fn, 128, "../features.float.%ld.4.data", nFeatures);
  LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(nFeatures * sizeof(struct Coordinate),
                                     (char *)coordinates, fn);
  printf("Loaded data.\n");

  // Randomize the centroids/neighbors.
  for (int64_t i = 0; i < sa1.nCentroids; ++i) {
    sa1.centroids[i] = rand() % nFeatures;
  }
  printf("Randomize centroids.\n");
  for (int64_t i = 0; i < sa1.nCentroids * sa1.nNeighbors; ++i) {
    sa1.neighbors[i] = rand() % nFeatures;
  }
  printf("Randomize neighbors.\n");

  // Don't bother initialize weights/buffers.

#ifdef GEM_FORGE

  {
#ifdef SPLIT_XYZ
    Value *x = (Value *)coordinates;
    Value *y = x + nFeatures;
    Value *z = y + nFeatures;
    Value *dist = sa1.distances;
    gf_stream_nuca_region("gfm.pntnet2.x", x, sizeof(Value), nFeatures);
    gf_stream_nuca_region("gfm.pntnet2.y", y, sizeof(Value), nFeatures);
    gf_stream_nuca_region("gfm.pntnet2.z", z, sizeof(Value), nFeatures);
    gf_stream_nuca_region("gfm.pntnet2.dist", dist, sizeof(Value), nFeatures);
    gf_stream_nuca_align(x, dist, 1);
    gf_stream_nuca_align(y, dist, 1);
    gf_stream_nuca_align(z, dist, 1);
#ifndef SAMPLE_USE_PUM
    gf_stream_nuca_set_property(x, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
    gf_stream_nuca_set_property(y, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
    gf_stream_nuca_set_property(z, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
    gf_stream_nuca_set_property(dist, STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
#endif
#endif

    Index *neighbors = sa1.neighbors;
    gf_stream_nuca_region("gfm.pntnet2.neighbor", neighbors, sizeof(Index),
                          nCentroids * nNeighbors);
    gf_stream_nuca_set_property(neighbors, STREAM_NUCA_REGION_PROPERTY_USE_PUM,
                                0);

    // Input buffer.
    gf_stream_nuca_region("gfm.pntnet2.buf0", sa1.buffers[0], sizeof(Value),
                          layerDims[0],
                          max(layerDims[0], nCentroids * nNeighbors));
    gf_stream_nuca_align(sa1.buffers[0], sa1.buffers[0], 1);
    gf_stream_nuca_align(sa1.buffers[0], sa1.buffers[0], layerDims[0]);
    for (int i = 1; i <= nLayers; ++i) {
      int64_t prevDim = layerDims[i - 1];
      int64_t myDim = layerDims[i];

      // Split weights into multiple regions.
      for (int j = 0; j < myDim / prevDim; ++j) {
        snprintf(fn, 128, "gfm.pntnet2.weight%d-%d", i - 1, j);
        Value *weight = (Value *)(sa1.weights[i - 1] + j * prevDim * prevDim);
        gf_stream_nuca_region(fn, weight, sizeof(Value), prevDim, prevDim);
        gf_stream_nuca_align(weight, weight, 1);
        gf_stream_nuca_align(weight, weight, prevDim);
      }

      if (i >= 2 && sa1.buffers[i - 2] == sa1.buffers[i]) {
        // Reuse buffer.
      } else {
        snprintf(fn, 128, "gfm.pntnet2.buf%d", i);
        gf_stream_nuca_region(fn, sa1.buffers[i], sizeof(Value), prevDim,
                              max(prevDim, nCentroids * nNeighbors));
        gf_stream_nuca_align(sa1.buffers[i], sa1.buffers[i - 1], 0);
      }
      // The last layer have aggregated results. No need to align.
      if (i == nLayers) {
        snprintf(fn, 128, "gfm.pntnet2.out%d", i);
        gf_stream_nuca_region(fn, sa1.outs[i], sizeof(Value), myDim,
                              nCentroids);
        gf_stream_nuca_set_property(sa1.outs[i],
                                    STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
      } else {
        if (myDim == prevDim) {
          // That's it.
        } else {
          snprintf(fn, 128, "gfm.pntnet2.out%d", i);
          gf_stream_nuca_region(fn, sa1.outs[i], sizeof(Value), myDim,
                                max(myDim, nCentroids * nNeighbors));
          gf_stream_nuca_align(sa1.outs[i], sa1.outs[i], 1);
          gf_stream_nuca_align(sa1.outs[i], sa1.outs[i], myDim);
        }
      }
    }

    //     Value *buffer = sa1.buffer;
    //     Value *gathered = sa1.gathered;
    //     Value *weights = (Value *)sa1.weights[0];
    //     gf_stream_nuca_region("gfm.pntnet2.buffer", buffer, sizeof(Value),
    //     nDims,
    //                           bufferRows);
    //     gf_stream_nuca_region("gfm.pntnet2.gathered", gathered,
    //     sizeof(Value),
    //                           nDims, bufferRows);
    //     gf_stream_nuca_region("gfm.pntnet2.weights", weights, sizeof(Value),
    //     nDims,
    //                           nDims * weightLayers);

    // #ifdef MLP_OUTER
    //     gf_stream_nuca_align(buffer, buffer, 1);
    //     gf_stream_nuca_align(buffer, buffer, nDims);
    //     gf_stream_nuca_align(gathered, buffer, 0);
    //     gf_stream_nuca_align(weights, weights, 1);
    //     gf_stream_nuca_align(weights, weights, nDims);
    // #endif

    //     Value *results = sa1.results;
    //     gf_stream_nuca_region("gfm.pntnet2.results", results, sizeof(Value),
    //     nDims,
    //                           nCentroids);
    //     gf_stream_nuca_set_property(results,
    //     STREAM_NUCA_REGION_PROPERTY_USE_PUM,
    //                                 0);
  }

  //   gf_stream_nuca_region("gfm.pointnet.features", features, sizeof(Value),
  //   nDims,
  //                         nFeatures);
  // gf_stream_nuca_region("gfm.pointnet.neighbors", neighbors, sizeof(Index),
  // 1,
  //                       nPoints);

  //   gf_stream_nuca_set_property(results,
  //   STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT,
  //                               1);
  //   gf_stream_nuca_set_property(temp,
  //   STREAM_NUCA_REGION_PROPERTY_PUM_NO_INIT, 1);
  //   gf_stream_nuca_set_property(features,
  //   STREAM_NUCA_REGION_PROPERTY_USE_PUM, 0);
  //   gf_stream_nuca_set_property(neighbors,
  //   STREAM_NUCA_REGION_PROPERTY_USE_PUM,
  //                               0);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    //     gf_warm_array("gfm.pointnet.results", results,
    //                   nPoints * nDims * sizeof(Value));
    //     gf_warm_array("gfm.pointnet.features", features,
    //                   nFeatures * nDims * sizeof(Value));
    //     gf_warm_array("gfm.pointnet.neighbors", neighbors, nPoints *
    //     sizeof(Index)); gf_warm_array("gfm.pointnet.weights", weights,
    //                   nDims * nDims * nLayers * sizeof(Value));
    //     gf_warm_array("gfm.pointnet.temp", temp, nDims * nPoints *
    //     sizeof(Value));
  }

#ifndef NO_OPENMP
// #pragma omp parallel for schedule(static)
//   for (int tid = 0; tid < numThreads; ++tid) {
//     volatile Value x = features[tid];
//   }
#endif

  gf_reset_stats();
#ifdef THREAD_INIT_ONLY
#pragma omp parallel
  { exit(0); }
#else
  volatile Value computed = foo(sa1);
#endif
  gf_detail_sim_end();

  return 0;
}
