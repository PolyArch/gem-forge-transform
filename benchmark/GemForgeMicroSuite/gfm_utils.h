#ifndef GEM_FORGE_MICRO_SUITE_UTILS_H
#define GEM_FORGE_MICRO_SUITE_UTILS_H

#define VALUE_TYPE_FLOAT 1
#define VALUE_TYPE_INT32 2
#define VALUE_TYPE_INT16 3
#define VALUE_TYPE_INT8 4

#ifndef VALUE_TYPE
#define VALUE_TYPE VALUE_TYPE_FLOAT
#endif

#if VALUE_TYPE == VALUE_TYPE_FLOAT

#define ValueT float
#define ValueAVX __m512
#define ValueAVXLoad _mm512_load_ps
#define ValueAVXStore _mm512_store_ps
#define ValueAVXMaskStore _mm512_mask_store_ps
#define ValueAVXAdd _mm512_add_ps
#define ValueAVXSub _mm512_sub_ps
#define ValueAVXMul _mm512_mul_ps
#define ValueAVXMin _mm512_min_ps
#define ValueAVXMax _mm512_max_ps
#define ValueAVXDiv _mm512_div_ps
#define ValueAVXReduceAdd _mm512_reduce_add_ps
#define ValueAVXSet1 _mm512_set1_ps

#elif VALUE_TYPE == VALUE_TYPE_INT32

#define ValueT int32_t
#define ValueAVX __m512i
#define ValueAVXLoad _mm512_load_epi32
#define ValueAVXStore _mm512_store_epi32
#define ValueAVXMaskStore _mm512_mask_store_epi32
#define ValueAVXAdd _mm512_add_epi32
#define ValueAVXSub _mm512_sub_epi32
#define ValueAVXMul _mm512_mul_epi32
#define ValueAVXReduceAdd _mm512_reduce_add_epi32
#define ValueAVXSet1 _mm512_set1_epi32
// There is no integer div :(

#elif VALUE_TYPE == VALUE_TYPE_INT16

#define ValueT int16_t
#define ValueAVX __m512i
#define ValueAVXLoad _mm512_loadu_epi16
#define ValueAVXStore _mm512_storeu_epi16
#define ValueAVXMaskStore _mm512_mask_store_epi16
#define ValueAVXAdd _mm512_adds_epi16
#define ValueAVXSub _mm512_subs_epi16
#define ValueAVXMul _mm512_mullo_epi16
// ! There are no reduce_add_epi16, faked with epi32.
#define ValueAVXReduceAdd _mm512_reduce_add_epi32
#define ValueAVXSet1 _mm512_set1_epi16
// There is no integer div :(

#elif VALUE_TYPE == VALUE_TYPE_INT8

#define ValueT int8_t
#define ValueAVX __m512i
#define ValueAVXLoad _mm512_loadu_epi8
#define ValueAVXStore _mm512_storeu_epi8
#define ValueAVXMaskStore _mm512_mask_store_epi8
#define ValueAVXAdd _mm512_adds_epi8
#define ValueAVXSub _mm512_subs_epi8
// ! There are no mul_epi8, faked with epi32.
#define ValueAVXMul _mm512_mul_epi32
// ! There are no reduce_add_epi16, faked with epi32.
#define ValueAVXReduceAdd _mm512_reduce_add_epi32
#define ValueAVXSet1 _mm512_set1_epi8
// There is no integer div :(

#else
#error "Unkown ValueT"

#endif

#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#else
#include <time.h>
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Used to readin an array and warm up the cache.
#define CACHE_LINE_SIZE 64
#define PAGE_SIZE 4096
#define WARM_UP_ARRAY(a, n)                                                    \
  for (int i = 0; i < sizeof(*(a)) * (n); i += CACHE_LINE_SIZE) {              \
    volatile char x = ((char *)(a))[i];                                        \
  }
#define DUMP_BIN_ARRAY_TO_FILE(a, n, f)                                        \
  {                                                                            \
    uint64_t s = sizeof(*(a)) * (n);                                           \
    fwrite(&s, sizeof(s), 1, f);                                               \
    fwrite((a), sizeof(*(a)), (n), (f));                                       \
  }

uint8_t *LOAD_BIN_ARRAY_FROM_FILE(uint64_t *size, FILE *f) {
  uint64_t s;
  fread(&s, sizeof(s), 1, f);
  uint8_t *buffer = (uint8_t *)aligned_alloc(PAGE_SIZE, s);
  fread(buffer, sizeof(*buffer), s, (f));
  *size = s;
  return buffer;
}

void LOAD_BIN_ARRAY_FROM_FILE_TO_BUFFER(int64_t size, char *buffer,
                                        const char *fn) {
  FILE *f = fopen(fn, "rb");
  assert(f);
  fread(buffer, sizeof(char), size, (f));
  fclose(f);
}

#ifdef GEM_FORGE
#define gf_detail_sim_start() m5_detail_sim_start()
#define gf_detail_sim_end() m5_detail_sim_end()
#define gf_reset_stats() m5_reset_stats(0, 0)
#define gf_dump_stats() m5_dump_stats(0, 0)
#define gf_panic() m5_panic()
#define gf_work_begin(x) m5_work_begin(x, 0)
#define gf_work_end(x) m5_work_end(x, 0)

#define gf_stream_nuca_region1d(name, start, elementSize, dim1)                \
  m5_stream_nuca_region(name, start, elementSize, dim1, 0, 0)
#define gf_stream_nuca_region2d(name, start, elementSize, dim1, dim2)          \
  m5_stream_nuca_region(name, start, elementSize, dim1, dim2, 0)
#define gf_stream_nuca_region3d(name, start, elementSize, dim1, dim2, dim3)    \
  m5_stream_nuca_region(name, start, elementSize, dim1, dim2, dim3)
#define get_4th_arg(name, start, elemSize, arg1, arg2, arg3, arg4, ...) arg4
#define gf_stream_nuca_region(...)                                             \
  get_4th_arg(__VA_ARGS__, gf_stream_nuca_region3d, gf_stream_nuca_region2d,   \
              gf_stream_nuca_region1d)(__VA_ARGS__)

#define gf_stream_nuca_align(A, B, elementOffset)                              \
  m5_stream_nuca_align(A, B, elementOffset)
#define gf_stream_nuca_set_property(start, property, value)                    \
  m5_stream_nuca_set_property(start, property, value)
#define gf_stream_nuca_remap() m5_stream_nuca_remap()

void gf_warm_array(const char *name, void *buffer, uint64_t totalBytes) {
  uint64_t cachedBytes = m5_stream_nuca_get_cached_bytes(buffer);
  printf("[GF_WARM] Region %s TotalBytes %lu CachedBytes %lu Cached %.2f%%.\n",
         name, totalBytes, cachedBytes,
         ((float)cachedBytes) / ((float)totalBytes) * 100.f);
  assert(cachedBytes <= totalBytes);
  for (uint64_t i = 0; i < cachedBytes; i += 64) {
    __attribute__((unused)) volatile uint8_t data = ((uint8_t *)buffer)[i];
  }
  printf("[GF_WARM] Region %s Warmed %.2f%%.\n", name,
         ((float)cachedBytes) / ((float)totalBytes) * 100.f);
}

#else
#define gf_detail_sim_start()
#define gf_detail_sim_end()
#define gf_reset_stats()
#define gf_dump_stats()
#define gf_panic() assert(0 && "gf_panic")
#define gf_stream_nuca_region(args...)
#define gf_stream_nuca_align(args...)
#define gf_stream_nuca_set_property(args...)
#define gf_stream_nuca_remap()
#define gf_work_begin(x)
#define gf_work_end(x)

void gf_warm_array(const char *name, void *buffer, uint64_t totalBytes) {
  printf("[GF_WARM] Region %s TotalBytes %lu WarmAll.\n", name, totalBytes);
  for (uint64_t i = 0; i < totalBytes; i += 64) {
    __attribute__((unused)) volatile uint8_t data = ((uint8_t *)buffer)[i];
  }
  printf("[GF_WARM] Region %s Warmed All.\n", name);
}

#endif

/****************************************************************
 * These are used as temporary StreamISA API for programmer.
 ****************************************************************/

typedef int32_t StreamIdT;
StreamIdT ssp_stream_config(void *start, int elementSize, int step);
void ssp_stream_end(StreamIdT stream);
int32_t ssp_stream_load_i32(StreamIdT stream);
void ssp_stream_step(StreamIdT stream, int step);

const char *formatBytes(uint64_t *bytes) {
  const char *suffix = "B";
  if ((*bytes) >= 1024 * 1024) {
    (*bytes) /= 1024 * 1024;
    suffix = "MB";
  } else if ((*bytes) >= 1024) {
    (*bytes) /= 1024;
    suffix = "kB";
  }
  return suffix;
}

static int alignBytes = 4096;
void *alignedAllocAndTouch(uint64_t numElements, int elemSize) {
  uint64_t totalBytes = elemSize * numElements;
  if (totalBytes % alignBytes) {
    totalBytes = (totalBytes / alignBytes + 1) * alignBytes;
  }
  char *p = (char *)aligned_alloc(alignBytes, totalBytes);

  for (unsigned long Byte = 0; Byte < totalBytes; Byte += alignBytes) {
    p[Byte] = 0;
  }
  return p;
}

#endif