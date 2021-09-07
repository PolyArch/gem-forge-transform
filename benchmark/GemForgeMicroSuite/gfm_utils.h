#ifndef GEM_FORGE_MICRO_SUITE_UTILS_H
#define GEM_FORGE_MICRO_SUITE_UTILS_H

#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#else
#include <assert.h>
#include <time.h>
#endif
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

#ifdef GEM_FORGE
#define gf_detail_sim_start() m5_detail_sim_start()
#define gf_detail_sim_end() m5_detail_sim_end()
#define gf_reset_stats() m5_reset_stats(0, 0)
#define gf_dump_stats() m5_dump_stats(0, 0)
#define gf_panic() m5_panic()
#define gf_stream_nuca_region(start, elementSize, numElement)                  \
  m5_stream_nuca_region(start, elementSize, numElement)
#define gf_stream_nuca_align(A, B, elementOffset)                              \
  m5_stream_nuca_align(A, B, elementOffset)
#define gf_stream_nuca_remap() m5_stream_nuca_remap()
#else
#define gf_detail_sim_start()
#define gf_detail_sim_end()
#define gf_reset_stats()
#define gf_dump_stats()
#define gf_panic() assert(0 && "gf_panic")
#define gf_stream_nuca_region(args...)
#define gf_stream_nuca_align(args...)
#define gf_stream_nuca_remap()
#endif

/****************************************************************
 * These are used as temporary StreamISA API for programmer.
 ****************************************************************/

typedef int32_t StreamIdT;
StreamIdT ssp_stream_config(void *start, int elementSize, int step);
void ssp_stream_end(StreamIdT stream);
int32_t ssp_stream_load_i32(StreamIdT stream);
void ssp_stream_step(StreamIdT stream, int step);

#endif