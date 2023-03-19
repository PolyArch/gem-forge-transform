#ifndef GEM_FORGE_OPS_H
#define GEM_FORGE_OPS_H

#include "gem5/m5ops.h"

#include <cstddef>

/**
 * Provide fake implementation for m5 operations.
 */

extern "C" void m5_stream_nuca_region(const char *regionName,
                                      const void *buffer, uint64_t elementSize,
                                      uint64_t dim1, uint64_t dim2,
                                      uint64_t dim3) {}

extern "C" void
m5_stream_nuca_set_property(const void *buffer,
                            enum StreamNUCARegionProperty property,
                            uint64_t value) {}

extern "C" uint64_t
m5_stream_nuca_get_property(const void *buffer,
                            enum StreamNUCARegionProperty property) {

  if (property == STREAM_NUCA_REGION_PROPERTY_BANK_ROWS) {
    return 8;
  }
  if (property == STREAM_NUCA_REGION_PROPERTY_BANK_COLS) {
    return 8;
  }
  if (property == STREAM_NUCA_REGION_PROPERTY_INTERLEAVE) {
    return 64;
  }
  if (property == STREAM_NUCA_REGION_PROPERTY_START_BANK) {
    return 0;
  }
  const size_t pageShift = 12;
  if (property == STREAM_NUCA_REGION_PROPERTY_START_VADDR) {
    return (reinterpret_cast<size_t>(buffer) >> pageShift) << pageShift;
  }
  if (property == STREAM_NUCA_REGION_PROPERTY_END_VADDR) {
    return ((reinterpret_cast<size_t>(buffer) >> pageShift) + 1) << pageShift;
  }

  assert(false && "Illegal RegionPropoerty");
}

extern "C" void m5_stream_nuca_remap() {}

#endif