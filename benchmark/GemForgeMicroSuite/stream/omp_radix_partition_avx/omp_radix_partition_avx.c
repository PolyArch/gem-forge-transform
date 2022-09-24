/**
 * Simple memmove.
 */
#include "gfm_utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "immintrin.h"

typedef int Value;
typedef uint64_t HistT;

// #define CHECK
#define WARM_CACHE

const int HistogramSize = 256;

/**
 * Partition the data according to the histogram distribution.
 * Copy and reorder the data into partition, with partition_pos[i] as the offset
 * to the ith bucket.
 */

__attribute__((noinline)) Value foo(Value *a, HistT *local_histograms,
                                    HistT *partition_pos, Value *partition,
                                    int64_t N) {
#pragma omp parallel firstprivate(a, local_histograms, partition_pos,          \
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
      ValueAVX shifted = _mm512_srli_epi32(val, 24);
      // Split into two.
      __m256i upper = _mm512_extracti32x8_epi32(shifted, 1);
      __m256i lower = _mm512_castsi512_si256(shifted);
      // Since we don't care the order, we just pack them.
      __m256i packed = _mm256_packus_epi32(upper, lower);
      // Do this again.
      __m128i upper2 = _mm256_extracti32x4_epi32(packed, 1);
      __m128i lower2 = _mm256_castsi256_si128(packed);
      __m128i result = _mm_packus_epi16(lower2, upper2);
      // Increment the histogram.
      local_histogram[_mm_extract_epi8(result, 0)]++;
      local_histogram[_mm_extract_epi8(result, 1)]++;
      local_histogram[_mm_extract_epi8(result, 2)]++;
      local_histogram[_mm_extract_epi8(result, 3)]++;
      local_histogram[_mm_extract_epi8(result, 4)]++;
      local_histogram[_mm_extract_epi8(result, 5)]++;
      local_histogram[_mm_extract_epi8(result, 6)]++;
      local_histogram[_mm_extract_epi8(result, 7)]++;
      local_histogram[_mm_extract_epi8(result, 8)]++;
      local_histogram[_mm_extract_epi8(result, 9)]++;
      local_histogram[_mm_extract_epi8(result, 10)]++;
      local_histogram[_mm_extract_epi8(result, 11)]++;
      local_histogram[_mm_extract_epi8(result, 12)]++;
      local_histogram[_mm_extract_epi8(result, 13)]++;
      local_histogram[_mm_extract_epi8(result, 14)]++;
      local_histogram[_mm_extract_epi8(result, 15)]++;
    }

    // Compute the prefix sum.
    HistT sum = 0;
    for (int64_t i = 0; i < HistogramSize; ++i) {
      HistT count = local_histogram[i];
      sum += count;
      local_histogram[i] = sum;
    }
#pragma omp barrier

    HistT local_partition_pos[HistogramSize];
    memset(local_partition_pos, 0, sizeof(HistT) * HistogramSize);
    // Collect histogram from other threads to determine my output position.
    for (int tid = 0; tid < thread_id; ++tid) {
      for (int bucket = 0; bucket < HistogramSize; ++bucket) {
        local_partition_pos[bucket] +=
            local_histograms[tid * HistogramSize + bucket];
      }
    }
    for (int tid = thread_id; tid < num_threads; ++tid) {
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

/**
 * Copy the data.
 * ! Use static scheduling to make sure we are getting the same data.
 */
#pragma omp for schedule(static)
    for (int64_t i = 0; i < N; i += CACHE_LINE_SIZE / sizeof(*a)) {
      // Load the value.
      ValueAVX val = ValueAVXLoad(a + i);
      // Zero-extend shift right to get the highest byte.
      ValueAVX shifted = _mm512_srli_epi32(val, 24);
      // Split into two.
      __m256i upper = _mm512_extracti32x8_epi32(shifted, 1);
      __m256i lower = _mm512_castsi512_si256(shifted);
      // Since we don't care the order, we just pack them.
      __m256i packed = _mm256_packus_epi32(lower, upper);
      // Do this again.
      __m128i upper2 = _mm256_extracti32x4_epi32(packed, 1);
      __m128i lower2 = _mm256_castsi256_si128(packed);
      __m128i result = _mm_packus_epi16(lower2, upper2);
      // Increment the histogram.
#define Move(value_idx, bucket_idx)                                            \
  {                                                                            \
    uint32_t value = _mm_extract_epi32(values, value_idx);                     \
    uint8_t bucket = _mm_extract_epi8(result, bucket_idx);                     \
    HistT pos = local_partition_pos[bucket];                                   \
    local_partition_pos[bucket]++;                                             \
    partition[pos] = value;                                                    \
  }
      {
        __m128i values = _mm512_extracti32x4_epi32(val, 0);
        Move(0, 0);
        Move(1, 1);
        Move(2, 2);
        Move(3, 3);
      }
      // The order is tricky due to the vpack instruction always pack at 128bit.
      {
        __m128i values = _mm512_extracti32x4_epi32(val, 1);
        Move(0, 8);
        Move(1, 9);
        Move(2, 10);
        Move(3, 11);
      }
      {
        __m128i values = _mm512_extracti32x4_epi32(val, 2);
        Move(0, 4);
        Move(1, 5);
        Move(2, 6);
        Move(3, 7);
      }
      {
        __m128i values = _mm512_extracti32x4_epi32(val, 3);
        Move(0, 12);
        Move(1, 13);
        Move(2, 14);
        Move(3, 15);
      }
    }
  }
  return 0;
}

// 65536*8 is 512kB.
const int64_t N = 24 * 1024 * 1024 / sizeof(Value);
// const int64_t N = 1 * 1024 / sizeof(Value);

HistT expected_pos[HistogramSize];

int main(int argc, char *argv[]) {

  int numThreads = 1;
  if (argc == 2) {
    numThreads = atoi(argv[1]);
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
  HistT *partition_pos =
      aligned_alloc(CACHE_LINE_SIZE, HistogramSize * sizeof(HistT));
#pragma clang loop vectorize(enable)
  for (long long i = 0; i < N; i++) {
    // Shuffle a little bit to make it not always linear access.
    long long shuffle = 0x5A;
    a[i] = (i ^ shuffle) << 24;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(partition, N);
  WARM_UP_ARRAY(partition_pos, HistogramSize);
  WARM_UP_ARRAY(histogram, HistogramSize * numThreads);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value c = foo(a, histogram, partition_pos, partition, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < HistogramSize; i++) {
    if (partition_pos[i] != expected_pos[i]) {
      for (int j = 0; j < HistogramSize; ++j) {
        printf("Hist %d Ret = %lu, Expected = %lu.\n", j, partition_pos[j],
               expected_pos[j]);
      }
      printf("Hist %d Ret = %lu, Expected = %lu.\n", i, partition_pos[i],
             expected_pos[i]);
      gf_panic();
    }
  }
  printf("Result correct.\n");
#endif

  return 0;
}
