#include "affinity_alloc.h"

#include "affinity_allocator.h"

extern "C" void *malloc_aff(size_t size, int n, const void **affinity_addrs) {

  /**
   * First get the list of affinity addresses.
   */
  affinity_alloc::AffinityAddressVecT affinityAddrs;

  for (int i = 0; i < n; ++i) {
    affinityAddrs.push_back(reinterpret_cast<uintptr_t>(affinity_addrs[i]));
  }

  return affinity_alloc::alloc(size, affinityAddrs);
}

extern "C" void free_aff(void *ptr) {
  // Currently we do nothing.
}