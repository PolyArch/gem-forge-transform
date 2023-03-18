#include "affinity_allocator.h"

#include <sys/types.h>
#include <unistd.h>

namespace affinity_alloc {

// Define the fixed sizes.
constexpr size_t FixNodeSizes[] = {
    64, 128, 256, 512, 1024, 4096,
};

// Specialize the allocator for predefined sizes.
constexpr size_t FixArenaSize = 1 * 1024 * 1024;
MultiThreadAffinityAllocator<64, 8192> allocator64B;
MultiThreadAffinityAllocator<128, FixArenaSize> allocator128B;
MultiThreadAffinityAllocator<256, FixArenaSize> allocator256B;
MultiThreadAffinityAllocator<512, FixArenaSize> allocator512B;
MultiThreadAffinityAllocator<1024, FixArenaSize> allocator1024B;
MultiThreadAffinityAllocator<4096, FixArenaSize> allocator4096B;

void *alloc(size_t size, const AffinityAddressVecT &affinityAddrs) {
  // Round up the size.
  auto roundSize = 0;
  for (auto x : FixNodeSizes) {
    if (size <= x) {
      roundSize = x;
      break;
    }
  }
  assert(roundSize != 0 && "Illegal size.");

  // Get the tid.
  int tid = gettid();

  // Dispatch to the correct allocator.
#define CASE(X)                                                                \
  case X:                                                                      \
    return allocator##X##B.alloc(tid, affinityAddrs);                          \
    break;
  switch (roundSize) {
    CASE(64);
    CASE(128);
    CASE(256);
    CASE(512);
    CASE(1024);
    CASE(4096);
  default:
    assert(false && "Illegal size.");
    break;
  }
#undef CASE
}

} // namespace affinity_alloc
