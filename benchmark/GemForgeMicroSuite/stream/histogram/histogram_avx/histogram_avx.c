/**
 * Simple memmove.
 */
#include "gfm_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "immintrin.h"

typedef int Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, uint64_t *histogram, int N) {
  for (int64_t i = 0; i < N; i += CACHE_LINE_SIZE / sizeof(*a)) {
    // Load the value.
    __m512i val = _mm512_load_epi32(a + i);
    // Zero-extend shift right to get the highest byte.
    __m512i shifted = _mm512_srli_epi32(val, 24);
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
    histogram[_mm_extract_epi8(result, 0)]++;
    histogram[_mm_extract_epi8(result, 1)]++;
    histogram[_mm_extract_epi8(result, 2)]++;
    histogram[_mm_extract_epi8(result, 3)]++;
    histogram[_mm_extract_epi8(result, 4)]++;
    histogram[_mm_extract_epi8(result, 5)]++;
    histogram[_mm_extract_epi8(result, 6)]++;
    histogram[_mm_extract_epi8(result, 7)]++;
    histogram[_mm_extract_epi8(result, 8)]++;
    histogram[_mm_extract_epi8(result, 9)]++;
    histogram[_mm_extract_epi8(result, 10)]++;
    histogram[_mm_extract_epi8(result, 11)]++;
    histogram[_mm_extract_epi8(result, 12)]++;
    histogram[_mm_extract_epi8(result, 13)]++;
    histogram[_mm_extract_epi8(result, 14)]++;
    histogram[_mm_extract_epi8(result, 15)]++;
  }
  return 0;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;

int main() {

  const int HistogramSize = 256;
  uint64_t histogram[HistogramSize];
  uint64_t expected[HistogramSize];
#pragma clang loop vectorize(disable)
  for (int64_t i = 0; i < HistogramSize; ++i) {
    expected[i] = 0;
    histogram[i] = 0;
  }

  Value *a = aligned_alloc(CACHE_LINE_SIZE, N * sizeof(Value));
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (long long i = 0; i < N; i++) {
    a[i] = i << 24;
    expected[i & 0xFF]++;
  }

  gf_detail_sim_start();
#ifdef WARM_CACHE
  WARM_UP_ARRAY(a, N);
  WARM_UP_ARRAY(histogram, HistogramSize);
#endif

  gf_reset_stats();
  volatile Value c = foo(a, histogram, N);
  gf_detail_sim_end();

#ifdef CHECK
  for (int i = 0; i < HistogramSize; i++) {
    if (histogram[i] != expected[i]) {
      for (int j = 0; j < HistogramSize; ++j) {
        printf("Hist %d Ret = %lu, Expected = %lu.\n", j, histogram[j],
               expected[j]);
      }
      printf("Hist %d Ret = %lu, Expected = %lu.\n", i, histogram[i],
             expected[i]);
      gf_panic();
    }
  }
  printf("Result correct.\n");
#endif

  return 0;
}
