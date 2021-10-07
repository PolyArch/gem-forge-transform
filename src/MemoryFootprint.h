
#ifndef LLVM_TDG_MEMORY_FOOTPRINT_H
#define LLVM_TDG_MEMORY_FOOTPRINT_H

#include <cstdint>
#include <list>
#include <unordered_map>

class MemoryFootprint {
public:
  MemoryFootprint(int _CacheLineInBytes = 64);
  MemoryFootprint(const MemoryFootprint &Other) = delete;
  MemoryFootprint(MemoryFootprint &&Other) = delete;
  MemoryFootprint &operator=(const MemoryFootprint &Other) = delete;
  MemoryFootprint &operator=(MemoryFootprint &&Other) = delete;

  using Address = uint64_t;

  void access(Address Addr);

  uint64_t getNumAccesses() const { return this->NumAccesses; }
  uint64_t getNumCacheLinesAccessed() const {
    return this->NumCacheLinesAccessed;
  }

  void clear();

private:
  static const size_t LOG_THRESHOLD = 1000000;

  using LogEntry = std::pair<Address, uint64_t>;
  std::list<LogEntry> Log;
  std::unordered_map<Address, uint64_t> Map;

  const int CacheLineInBytes;
  uint64_t NumCacheLinesAccessed;
  uint64_t NumAccesses;

  uint64_t AccessId;

  void release();
};

#endif
