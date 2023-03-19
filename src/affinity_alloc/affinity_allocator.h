#ifndef AFFINITY_ALLOCATOR_HH
#define AFFINITY_ALLOCATOR_HH

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <vector>

// #include <immintrin.h>

#include "gem5/m5ops.h"

namespace affinity_alloc {

constexpr bool isInGemForge() {
#ifdef GEM_FORGE
  return true;
#else
  return false;
#endif
}

namespace {
int countBits(int v) {
  // v should be a power of 2 and positive.
  int bits = 0;
  v--;
  while (v) {
    bits++;
    v >>= 1;
  }
  return bits;
}
} // namespace

using Addr = std::uintptr_t;
using AffinityAddressVecT = std::vector<Addr>;

/**
 * @brief
 * This template assumes a bidirection linked list,
 * i.e. with next and prev pointers.
 *
 * It allocates space in the granularity of arena from malloc.
 *
 * @tparam NodeT
 */
template <int NodeSize, int ArenaSize> class AffinityAllocator {
public:
  constexpr static int MaxBanks = 64;

  static_assert(NodeSize >= 64, "Invalid NodeSize");

  struct NodeT {
    NodeT *prev = nullptr;
    NodeT *next = nullptr;
    char remain[NodeSize - 16];
  };

  AffinityAllocator()
      : numRows(m5_stream_nuca_get_property(
            nullptr, STREAM_NUCA_REGION_PROPERTY_BANK_ROWS)),
        rowMask(numRows - 1), rowBits(countBits(numRows)),
        numCols(m5_stream_nuca_get_property(
            nullptr, STREAM_NUCA_REGION_PROPERTY_BANK_COLS)),
        colMask(numCols - 1), colBits(countBits(numCols)),
        totalBanks(numRows * numCols), bankFreeList(totalBanks, nullptr) {
    assert(ArenaSize > totalBanks && "Arena too small.");
    assert(numRows >= 1);
    assert((numRows & (numRows - 1)) == 0);
    assert(numCols >= 1);
    assert((numCols & (numCols - 1)) == 0);
    assert(totalBanks <= MaxBanks);

    this->initBankToBankHops();
  }

  AffinityAllocator(const AffinityAllocator &other) = delete;
  AffinityAllocator &operator=(const AffinityAllocator &other) = delete;
  AffinityAllocator(AffinityAllocator &&other) = delete;
  AffinityAllocator &operator=(AffinityAllocator &&other) = delete;

  ~AffinityAllocator() { this->deallocArenas(); }

  NodeT *alloc(const AffinityAddressVecT &affinityAddrs) {

    auto allocBank = this->chooseAllocBank(affinityAddrs);

    if (!this->bankFreeList.at(allocBank)) {
      this->allocArena();
    }

    return this->popBankFreeList(allocBank);
  }

  /*********************************************************
   * Some internal data structure used to manage free space.
   *********************************************************/

  struct AffinityAllocatorArena {
    NodeT data[ArenaSize];
    AffinityAllocatorArena *next = nullptr;
    AffinityAllocatorArena *prev = nullptr;

    AffinityAllocatorArena() = default;
  };

  /*************************************************************
   * Memorized region information.
   *************************************************************/
  struct RegionInfo {
    const Addr lhs;
    const Addr rhs;
    const Addr interleave;
    const int startBank;
    RegionInfo(Addr _lhs, Addr _rhs, Addr _interleave, int _startBank)
        : lhs(_lhs), rhs(_rhs), interleave(_interleave), startBank(_startBank) {
    }
    int calculateBank(Addr vaddr, int totalBanks) const {
      auto diff = vaddr - this->lhs;
      auto bank = diff / this->interleave;
      return (this->startBank + bank) % totalBanks;
    }
  };
  const int numRows;
  const int rowMask;
  const int rowBits;
  const int numCols;
  const int colMask;
  const int colBits;
  const int totalBanks;
  using RegionInfoMap = std::map<Addr, RegionInfo>;
  using RegionInfoMapIter = typename std::map<Addr, RegionInfo>::iterator;
  RegionInfoMap vaddrRegionMap;

  /**
   * We use uint8_t for hops. And optimize the hops computation.
   */
  using HopT = uint8_t;
  struct BankHopT {
    std::array<HopT, MaxBanks> hops;
  };
  std::array<BankHopT, MaxBanks> bankToBankHops;

  // Buffer to host distance to each bank.
  BankHopT hopsToEachBank;

  void initBankToBankHops() {
    for (int bankA = 0; bankA < this->totalBanks; ++bankA) {
      auto &bankHop = this->bankToBankHops[bankA];
      for (int bankB = 0; bankB < this->totalBanks; ++bankB) {
        bankHop.hops[bankB] = this->computeHops(bankA, bankB);
      }
    }
  }

  void resetHopsToEachBank() {
    for (int bank = 0; bank < this->totalBanks; ++bank) {
      hopsToEachBank.hops[bank] = 0;
    }
  }

  // Free list at each bank.
  std::vector<NodeT *> bankFreeList;

  void pushBankFreeList(int bank, NodeT *node) {
    auto &head = bankFreeList.at(bank);
    if (head) {
      head->prev = node;
    }
    node->next = head;
    head = node;
  }

  NodeT *popBankFreeList(int bank) {
    auto &head = this->bankFreeList.at(bank);
    assert(head);
    auto ret = head;
    head = head->next;
    if (head) {
      head->prev = nullptr;
    }
    return ret;
  }

  RegionInfoMapIter initRegionInfo(Addr vaddr) {
    auto ptr = reinterpret_cast<void *>(vaddr);
    Addr lhs = m5_stream_nuca_get_property(
        ptr, STREAM_NUCA_REGION_PROPERTY_START_VADDR);
    Addr rhs =
        m5_stream_nuca_get_property(ptr, STREAM_NUCA_REGION_PROPERTY_END_VADDR);
    Addr interleave = m5_stream_nuca_get_property(
        ptr, STREAM_NUCA_REGION_PROPERTY_INTERLEAVE);
    int startBank = m5_stream_nuca_get_property(
        ptr, STREAM_NUCA_REGION_PROPERTY_START_BANK);
    return this->vaddrRegionMap
        .emplace(std::piecewise_construct, std::forward_as_tuple(lhs),
                 std::forward_as_tuple(lhs, rhs, interleave, startBank))
        .first;
  }

  const RegionInfo &getOrInitRegionInfo(Addr vaddr) {
    auto iter = this->vaddrRegionMap.upper_bound(vaddr);
    if (iter == this->vaddrRegionMap.begin()) {
      iter = this->initRegionInfo(vaddr);
    } else {
      iter--;
      if (vaddr >= iter->second.rhs) {
        iter = this->initRegionInfo(vaddr);
      }
    }
    const auto &region = iter->second;
    return region;
  }

  /**
   * @brief Get the mapped bank for a give vaddr.
   *
   * @param vaddr
   * @return int
   */
  int getBank(Addr vaddr) {
    return this->getOrInitRegionInfo(vaddr).calculateBank(vaddr,
                                                          this->totalBanks);
  }

  int64_t computeHops(int64_t bankA, int64_t bankB) {
    int64_t bankARow = (bankA >> this->colBits) & this->rowMask;
    int64_t bankACol = (bankA & this->colMask);
    int64_t bankBRow = (bankB >> this->colBits) & this->rowMask;
    int64_t bankBCol = (bankB & this->colMask);
    return std::abs(bankARow - bankBRow) + std::abs(bankACol - bankBCol);
  }

  void computeHopsToEachBank(const AffinityAddressVecT &affinityAddrs) {
    // Clear the hops.
    this->resetHopsToEachBank();

    auto &sumHops = this->hopsToEachBank.hops;
    for (const auto vaddr : affinityAddrs) {
      auto bankA = this->getBank(vaddr);

      const auto &hops = this->bankToBankHops[bankA].hops;
#ifdef GEM_FORGE
    asm(
      "vmovdqa32      (%0), %%zmm0\n\t"
      "vpaddb         (%1), %%zmm0, %%zmm0\n\t"
      "vmovdqa32      %%zmm0, (%0)\n\t"
        : /* no output operands */
        : "r"(&sumHops), "r"(&hops)
        : "%zmm0"
    );
#else
      for (int bankB = 0; bankB < this->MaxBanks; ++bankB) {
        sumHops[bankB] += hops[bankB];
      }
#endif
    }
  }

  int chooseAllocBank(const AffinityAddressVecT &affinityAddrs) {
    /**
     * For now we have a simple policy: allocate at the bank with minimal hops.
     */
    this->computeHopsToEachBank(affinityAddrs);
    auto allocBank = 0;
    auto minHops = INT32_MAX;
    auto minHopsCnt = 0;
    for (int bank = 0; bank < this->totalBanks; ++bank) {
      auto hops = this->hopsToEachBank.hops[bank];
      if (hops < minHops) {
        minHops = hops;
        allocBank = bank;
        minHopsCnt = 1;
      } else if (hops == minHops) {
        // Randomly pick one bank with reservoir sampling.
        minHopsCnt++;
        // This is biased, but I don't care.
        bool replace = (rand() % minHopsCnt) == 0;
        allocBank = replace ? bank : allocBank;
      }
    }
    return allocBank;
  }

  AffinityAllocatorArena *arenas = nullptr;

  NodeT *allocFromArena() {
    if (!this->arenas || this->arenas->usedNodes == ArenaSize) {
      this->allocArena();
    }
    auto newNode = this->arenas->freeList;
    this->arenas->freeList = newNode->next;
    newNode->next = nullptr;
    if (this->arenas->freeList) {
      this->arenas->freeList->prev = nullptr;
    }
    this->arenas->usedNodes++;
    return newNode;
  }

  void allocArena() {

    auto arenaRaw = alignedAllocAndTouch<AffinityAllocatorArena>(1);

    auto arena = new (arenaRaw) AffinityAllocatorArena();

    /**
     * Register ourselve at StreamNUCAManger, and make sure that we are
     * interleaved at the NodeT with StartBank 0.
     * Then remember the RegionInfo.
     */
    {
      auto regionName = "affinity_alloc/";
      m5_stream_nuca_region(regionName, arena, sizeof(NodeT), ArenaSize, 0, 0);
      m5_stream_nuca_set_property(arena, STREAM_NUCA_REGION_PROPERTY_INTERLEAVE,
                                  NodeSize);
      m5_stream_nuca_set_property(arena, STREAM_NUCA_REGION_PROPERTY_START_BANK,
                                  0);
      m5_stream_nuca_remap();
      Addr lhs = reinterpret_cast<Addr>(arena);
      Addr rhs = reinterpret_cast<Addr>(arena + 1);
      this->vaddrRegionMap.emplace(
          std::piecewise_construct, std::forward_as_tuple(lhs),
          std::forward_as_tuple(lhs, rhs, NodeSize, 0));
    }

    // Connect arenas together.
    arena->next = this->arenas;
    if (this->arenas) {
      this->arenas->prev = arena;
    }
    this->arenas = arena;

    for (int64_t i = 0; i < ArenaSize; ++i) {
#ifdef GEM_FORGE
      // Add nodes from this arena to the free list.
      auto newNode = new (arena->data + i) NodeT();
#else
      // Just add them to the only one free list.
      auto newNode = new (arena->data + initFreeNodeIndexes.indexes[i]) NodeT();
#endif
      auto newBank = this->getBank(reinterpret_cast<Addr>(newNode));
      this->pushBankFreeList(newBank, newNode);
    }
  }

  void deallocArenas() {
    while (this->arenas) {
      auto arena = this->arenas;
      this->arenas = this->arenas->next;
      free(arena);
    }
  }

  static constexpr std::size_t alignBytes = 4096;
  template <typename T> T *alignedAllocAndTouch(size_t numElements) {
    auto totalBytes = sizeof(T) * numElements;
    if (totalBytes % alignBytes) {
      totalBytes = (totalBytes / alignBytes + 1) * alignBytes;
    }
    auto p = reinterpret_cast<T *>(aligned_alloc(alignBytes, totalBytes));

    auto raw = reinterpret_cast<char *>(p);
    for (unsigned long Byte = 0; Byte < totalBytes; Byte += alignBytes) {
      raw[Byte] = 0;
    }
    return p;
  }

  struct InitFreeNodeIndexes {
    std::array<int, ArenaSize> indexes;
    InitFreeNodeIndexes() {
      for (int i = 0; i < ArenaSize; ++i) {
        indexes[i] = ArenaSize - i - 1;
      }
#ifdef RANDOMIZE_ADJ_LIST
      for (int i = 0; i + 1 < ArenaSize; ++i) {
        // Randomize the free list.
        long long j = (rand() % (ArenaSize - i)) + i;
        auto tmp = indexes[i];
        indexes[i] = indexes[j];
        indexes[j] = tmp;
      }
#endif
    }
  };

  static InitFreeNodeIndexes initFreeNodeIndexes;
};

template <int NodeSize, int ArenaSize>
typename AffinityAllocator<NodeSize, ArenaSize>::InitFreeNodeIndexes
    AffinityAllocator<NodeSize, ArenaSize>::initFreeNodeIndexes;

template <int NodeSize, int ArenaSize> class MultiThreadAffinityAllocator {

public:
  using Allocator = AffinityAllocator<NodeSize, ArenaSize>;

  constexpr static int MaxNumThreads = 128;

  MultiThreadAffinityAllocator() = default;

  void *alloc(int tid, const AffinityAddressVecT &affinityAddrs) {
    auto allocator = this->getAllocator(tid);
    if (!allocator) {
      allocator = this->initAllocator(tid);
    }
    return allocator->alloc(affinityAddrs);
  }

  Allocator *getAllocator(int tid) {
    std::shared_lock lock(mutex);
    auto iter = allocators.find(tid);
    if (iter == allocators.end()) {
      return nullptr;
    } else {
      return iter->second;
    }
  }

  Allocator *initAllocator(int tid) {
    std::unique_lock lock(mutex);
    auto iter = allocators.emplace(tid, new Allocator());
    return iter.first->second;
  }

  mutable std::shared_mutex mutex;
  std::map<int, Allocator *> allocators;
};

void *alloc(size_t size, const AffinityAddressVecT &affinityAddrs);

} // namespace affinity_alloc

#endif