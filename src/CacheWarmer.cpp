#include "CacheWarmer.h"

#include <cassert>
#include <fstream>

CacheWarmer::CacheWarmer(const std::string &_FileName,
                         const size_t _CacheLineSize, const size_t _CacheSize)
    : FileName(_FileName), CacheLineSize(_CacheLineSize),
      CacheSize(_CacheSize) {
  assert(((this->CacheLineSize & (this->CacheLineSize - 1)) == 0) &&
         "Cache line size must be a power of 2.");
}

void CacheWarmer::addAccess(uint64_t Addr) {
  auto MaskedAddr = Addr & (~(this->CacheLineSize - 1));
  auto RecordIter = this->Record.insert(this->Record.end(), MaskedAddr);
  auto RecordMapIter = this->AddrToRecordMap.find(MaskedAddr);
  if (RecordMapIter == this->AddrToRecordMap.end()) {
    this->AddrToRecordMap.emplace(MaskedAddr, RecordIter);
  } else {
    this->Record.erase(RecordMapIter->second);
    RecordMapIter->second = RecordIter;
  }
  while (this->CacheSize / this->CacheLineSize < this->AddrToRecordMap.size()) {
    assert(!this->Record.empty() &&
           "Empty record list when there is address record.");
    auto OldestAddr = this->Record.front();
    this->Record.pop_front();
    this->AddrToRecordMap.erase(OldestAddr);
  }
}

void CacheWarmer::dumpToFile() const {
  std::ofstream WarmerFile(this->FileName);
  assert(WarmerFile.is_open() && "Failed to open cache warmer file.");
  for (const auto &Addr : this->Record) {
    WarmerFile << std::hex << Addr << '\n';
  }
  WarmerFile.close();
}