#ifndef GEM_FORGE_MICRO_SUITE_UTILS_H
#define GEM_FORGE_MICRO_SUITE_UTILS_H

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

#ifdef GEM_FORGE
#define gf_detail_sim_start() m5_detail_sim_start()
#define gf_detail_sim_end() m5_detail_sim_end()
#define gf_reset_stats() m5_reset_stats(0, 0)
#define gf_dump_stats() m5_dump_stats(0, 0)
#define gf_panic() m5_panic()
#define gf_stream_nuca_region(name, start, elementSize, numElement)            \
  m5_stream_nuca_region(name, start, elementSize, numElement)
#define gf_stream_nuca_align(A, B, elementOffset)                              \
  m5_stream_nuca_align(A, B, elementOffset)
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
#define gf_stream_nuca_remap()

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

#endif