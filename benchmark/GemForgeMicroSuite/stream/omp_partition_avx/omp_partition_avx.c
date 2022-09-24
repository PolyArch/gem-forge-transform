/**
 * Simple memmove.
 */
#include "gfm_utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "immintrin.h"

typedef uint32_t Value;
typedef uint32_t HistT;

#define CHECK
#define WARM_CACHE

typedef uint16_t HashKeyT;
#define HashKeyBits 15
#define HashKeyShift (8 * sizeof(Value) - HashKeyBits)
const int64_t HistogramSize = 1 << HashKeyBits;

/**
 * Partition the data according to the provided histogram.
 * It does not build the histograom.
 * This one uses simple indirect access to copy the data to the bucket.
 * We use 16 bit hash key -> 64kB * sizeof(HistT) = 256kB.
 */

__attribute__((noinline)) Value foo(Value *a, HistT *partition_pos,
                                    Value *partition, int64_t N) {
#pragma omp parallel firstprivate(a, partition_pos, partition, N)
  {
    int thread_id = omp_get_thread_num();
    int num_threads = omp_get_num_threads();
    HistT *local_partition_pos = partition_pos + thread_id * HistogramSize;

/**
 * Copy the data.
 * ! Use static scheduling to make sure we are getting the same data.
 */
#pragma omp for schedule(static)
    for (int64_t i = 0; i < N; i++) {
      Value value = a[i];
      HashKeyT bucket = value >> HashKeyShift;
      HistT pos = local_partition_pos[bucket];
      local_partition_pos[bucket]++;
      partition[pos] = value;
    }
  }
  return 0;
}

__attribute__((noinline)) Value
build_histogram(Value *a, HistT *local_histograms,
                HistT *local_partition_positions, HistT *partition_pos,
                Value *partition, int64_t N) {
#pragma omp parallel firstprivate(a, local_histograms,                         \
                                  local_partition_positions, partition_pos,    \
                                  partition, N)
  {
    int thread_id = omp_get_thread_num();
    int num_threads = omp_get_num_threads();
    HistT *local_histogram = local_histograms + thread_id * HistogramSize;

/**
 * Build the histogram.
 * ! Use static scheduling to make sure we are getting the same data.
 */
#pragma omp for nowait schedule(static)
    for (int64_t i = 0; i < N; i += CACHE_LINE_SIZE / sizeof(*a)) {
      // Load the value.
      ValueAVX val = ValueAVXLoad(a + i);
      // Zero-extend shift right to get the highest byte.
      ValueAVX shifted = _mm512_srli_epi32(val, HashKeyShift);
      // Split into two.
      __m256i upper = _mm512_extracti32x8_epi32(shifted, 1);
      __m256i lower = _mm512_castsi512_si256(shifted);
      // Since we don't care the order, we just pack them.
      __m256i packed = _mm256_packus_epi32(upper, lower);
      // Increment the histogram.
      local_histogram[_mm256_extract_epi16(packed, 0)]++;
      local_histogram[_mm256_extract_epi16(packed, 1)]++;
      local_histogram[_mm256_extract_epi16(packed, 2)]++;
      local_histogram[_mm256_extract_epi16(packed, 3)]++;
      local_histogram[_mm256_extract_epi16(packed, 4)]++;
      local_histogram[_mm256_extract_epi16(packed, 5)]++;
      local_histogram[_mm256_extract_epi16(packed, 6)]++;
      local_histogram[_mm256_extract_epi16(packed, 7)]++;
      local_histogram[_mm256_extract_epi16(packed, 8)]++;
      local_histogram[_mm256_extract_epi16(packed, 9)]++;
      local_histogram[_mm256_extract_epi16(packed, 10)]++;
      local_histogram[_mm256_extract_epi16(packed, 11)]++;
      local_histogram[_mm256_extract_epi16(packed, 12)]++;
      local_histogram[_mm256_extract_epi16(packed, 13)]++;
      local_histogram[_mm256_extract_epi16(packed, 14)]++;
      local_histogram[_mm256_extract_epi16(packed, 15)]++;
    }

    // Compute the prefix sum.
    HistT sum = 0;
#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t i = 0; i < HistogramSize; ++i) {
      HistT count = local_histogram[i];
      sum += count;
      local_histogram[i] = sum;
    }
#pragma omp barrier

    HistT *local_partition_pos =
        local_partition_positions + thread_id * HistogramSize;
    memset(local_partition_pos, 0, sizeof(HistT) * HistogramSize);
    // Collect histogram from other threads to determine my output position.
#pragma clang loop unroll(disable) interleave(disable)
    for (int64_t tid = 0; tid < thread_id; ++tid) {
#pragma clang loop unroll(disable) interleave(disable)
      for (int64_t bucket = 0; bucket < HistogramSize; ++bucket) {
        local_partition_pos[bucket] +=
            local_histograms[tid * HistogramSize + bucket];
      }
    }
#pragma clang loop unroll(disable) interleave(disable)
    for (int tid = thread_id; tid < num_threads; ++tid) {
#pragma clang loop unroll(disable) interleave(disable)
      for (int bucket = 1; bucket < HistogramSize; ++bucket) {
        HistT prevBucketSize =
            local_histograms[tid * HistogramSize + bucket - 1];
        local_partition_pos[bucket] += prevBucketSize;
      }
    }
    /**
     * If I am the first thread, I will copy my local partition_pos as the final
     * partition_pos.
     */
    if (thread_id == 0) {
      memcpy(partition_pos, local_partition_pos, sizeof(HistT) * HistogramSize);
    }
  }
  return 0;
}

HistT expected_pos[HistogramSize];

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc >= 2) {
    numThreads = atoi(argv[1]);
  }
  int64_t N = 1 * 1024 * 1024 / sizeof(Value);
  if (argc >= 3 && strcmp(argv[2], "large") == 0) {
    N = 4 * 1024 * 1024 / sizeof(Value);
  }
  printf("Number of Threads: %d.\n", numThreads);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  size_t histogram_bytes = sizeof(HistT) * HistogramSize * numThreads;
  HistT *histogram = aligned_alloc(CACHE_LINE_SIZE, histogram_bytes);
  memset(histogram, 0, histogram_bytes);

#pragma clang loop vectorize(enable)
  for (long long i = 0; i < HistogramSize; i++) {
    expected_pos[i] = i * (N / HistogramSize);
  }

  Value *a = aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  Value *partition = aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
  HistT *local_partition_positions =
      aligned_alloc(CACHE_LINE_SIZE, histogram_bytes);
  HistT *partition_pos =
      aligned_alloc(CACHE_LINE_SIZE, HistogramSize * sizeof(HistT));
#pragma clang loop vectorize(enable)
  for (long long i = 0; i < N; i++) {
    a[i] = i << HashKeyShift;
  }
  for (long long i = 0; i + 1 < N; i++) {
    // Shuffle a little bit to make it not always linear access.
    long long j = (rand() % (N - i)) + i;
    Value tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
  }
  memset(local_partition_positions, 0, histogram_bytes);

  build_histogram(a, histogram, local_partition_positions, partition_pos,
                  partition, N);

#ifdef CHECK
  /**
   * Check that the pos is correct.
   */
  // {
  //   if (numThreads == 1) {
  //     HistT expected_bucket_size = N / HistogramSize;
  //     for (int i = 0; i < HistogramSize; i++) {
  //       // We have the prefix sum here.
  //       if (histogram[i] != expected_bucket_size * (i + 1)) {
  //         printf("Histogram %d Ret = %u, Expected = %u.\n", i, histogram[i],
  //                expected_bucket_size * (i + 1));
  //         gf_panic();
  //       }
  //     }
  //     printf("Histogram correct.\n");
  //   }
  //   for (int i = 0; i < HistogramSize; i++) {
  //     if (partition_pos[i] != expected_pos[i]) {
  //       printf("PartitionPos %d Ret = %u, Expected = %u.\n", i,
  //              partition_pos[i], expected_pos[i]);
  //       gf_panic();
  //     }
  //   }
  //   printf("PartitionPos correct.\n");
  // }
#endif

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(partition, N);
  WARM_UP_ARRAY(local_partition_positions, numThreads * HistogramSize);
  // Threads already started.
#endif

  gf_reset_stats();
  volatile Value c = foo(a, local_partition_positions, partition, N);
  gf_detail_sim_end();

#ifdef CHECK
  /**
   * Check that the last thread has correct local_partition_positions.
   */
  {
    HistT *last_local_pos =
        local_partition_positions + (numThreads - 1) * HistogramSize;
    for (int i = 0; i + 1 < HistogramSize; i++) {
      if (last_local_pos[i] != expected_pos[i + 1]) {
        for (int j = 0; j + 1 < HistogramSize; ++j) {
          printf("Hist %d Ret = %u, Expected = %u.\n", j, last_local_pos[j],
                 expected_pos[j + 1]);
        }
        printf("Hist %d Ret = %u, Expected = %u.\n", i, last_local_pos[i],
               expected_pos[i + 1]);
        gf_panic();
      }
    }
  }
  printf("Result correct.\n");
#endif

  return 0;
}
