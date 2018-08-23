
#include "MemoryFootprint.h"

#include <cassert>

MemoryFootprint::MemoryFootprint(int _CacheLineInBytes)
    : CacheLineInBytes(_CacheLineInBytes), NumCacheLinesAccessed(0),
      NumAccesses(0), AccessId(0) {
  auto IsPowerOfTwo =
      this->CacheLineInBytes &&
      (!(this->CacheLineInBytes & (this->CacheLineInBytes - 1)));
  assert(IsPowerOfTwo && "Cache line should be a power of 2.");
}

void MemoryFootprint::access(Address Addr) {
  Addr &= ~(this->CacheLineInBytes - 1);
  auto Id = this->AccessId++;
  this->Log.emplace_back(Addr, Id);
  auto IsNew = this->Map.emplace(Addr, Id).second;
  if (IsNew) {
    // New cache line touched.
    this->NumCacheLinesAccessed++;
  }
  this->NumAccesses++;
  this->release();
}

void MemoryFootprint::release() {
  while (this->Log.size() > MemoryFootprint::LOG_THRESHOLD) {
    const auto &Entry = this->Log.front();
    auto Iter = this->Map.find(Entry.first);
    if (Iter != this->Map.end() && Iter->second == Entry.second) {
      this->Map.erase(Iter);
    }
    this->Log.pop_front();
  }
}