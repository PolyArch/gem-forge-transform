#include "affinity_alloc.h"

#include <cstdarg>
#include <vector>

#include "affinity_allocator.h"

extern "C" void *malloc_aff(size_t size, ...) {

  /**
   * First get the list of affinity addresses.
   */
  affinity_alloc::AffinityAddressVecT affinityAddrs;

  std::va_list args;
  va_start(args, size);
  void *address = va_arg(args, void *);
  while (address != NULL) {
    affinityAddrs.push_back(reinterpret_cast<std::uintptr_t>(address));
    address = va_arg(args, void *);
  }
  va_end(args);

  return affinity_alloc::alloc(size, affinityAddrs);
}

extern "C" void free_aff(void *ptr) {
  // Currently we do nothing.
}