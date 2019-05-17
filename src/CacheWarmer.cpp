#include "CacheWarmer.h"

#include "Gem5ProtobufSerializer.h"
#include "Utils.h"

#include <cassert>
#include <fstream>

CacheWarmer::CacheWarmer(const std::string &_ExtraFolder,
                         const std::string &_FileName,
                         const size_t _CacheLineSize, const size_t _CacheSize)
    : FileName(_FileName), SnapshotFileName(_ExtraFolder + "/mem.snap"),
      CacheLineSize(_CacheLineSize), CacheSize(_CacheSize) {
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

void CacheWarmer::addAccess(DynamicInstruction *DynInst,
                            llvm::DataLayout *DataLayout) {
  auto StaticInst = DynInst->getStaticInstruction();
  assert(Utils::isMemAccessInst(StaticInst) &&
         "Non-MemAccessInst passed to CacheWarmer.");
  auto Addr = Utils::getMemAddr(DynInst);
  // Normal cache warmup request.
  this->addAccess(Addr);
  // Check for the initial memory snapshot.
  if (llvm::isa<llvm::LoadInst>(StaticInst)) {
    // This is a load, check for any first accessed byte.
    const auto &LoadedValue = DynInst->DynamicResult->Value;
    auto TypeSize = DataLayout->getTypeStoreSize(StaticInst->getType());
    assert(TypeSize <= LoadedValue.size() &&
           "LoadedValue's size is too small.");
    auto &Snapshot = this->InitialMemorySnapshot;
    for (int Offset = 0; Offset < TypeSize; ++Offset) {
      auto ByteAddr = Addr + Offset;
      if (Snapshot.count(ByteAddr) == 0) {
        // This is a new initial byte.
        Snapshot[ByteAddr] = LoadedValue[Offset];
        // const uint64_t interestingAddr = 0x88707c;
        // if (ByteAddr >= interestingAddr && ByteAddr < interestingAddr + 4) {
        //   llvm::errs() << ByteAddr << ' ' << (uint32_t)(LoadedValue[Offset])
        //                << ' ' << Snapshot[ByteAddr] << ' '
        //                << DynInst->DynamicResult->getInt() << '\n';
        // }
      }
    }
  }
}

LLVM::TDG::MemorySnapshot CacheWarmer::generateSnapshot() const {
  std::vector<uint64_t> ByteAddrs;
  ByteAddrs.reserve(this->InitialMemorySnapshot.size());
  for (const auto &AddrByte : this->InitialMemorySnapshot) {
    ByteAddrs.push_back(AddrByte.first);
  }
  std::sort(ByteAddrs.begin(), ByteAddrs.end());
  LLVM::TDG::MemorySnapshot Snapshot;

  auto PreviousEntryIter = Snapshot.mutable_snapshot()->end();

  for (const auto &ByteAddr : ByteAddrs) {
    if (PreviousEntryIter != Snapshot.mutable_snapshot()->end()) {
      if (PreviousEntryIter->first + PreviousEntryIter->second.size() ==
          ByteAddr) {
        // Continuous.
        PreviousEntryIter->second.push_back(
            this->InitialMemorySnapshot.at(ByteAddr));
        continue;
      }
    }
    // All other cases, add an new entry.
    Snapshot.mutable_snapshot()->operator[](ByteAddr) = "";
    PreviousEntryIter = Snapshot.mutable_snapshot()->find(ByteAddr);
    PreviousEntryIter->second.push_back(
        this->InitialMemorySnapshot.at(ByteAddr));
  }
  return Snapshot;
}

void CacheWarmer::dumpToFile() const {
  std::ofstream WarmerFile(this->FileName);
  assert(WarmerFile.is_open() && "Failed to open cache warmer file.");
  for (const auto &Addr : this->Record) {
    WarmerFile << std::hex << Addr << '\n';
  }
  WarmerFile.close();
  // Dump the memory snap file.
  Gem5ProtobufSerializer SnapshotSerializer(this->SnapshotFileName);
  SnapshotSerializer.serialize(this->generateSnapshot());
}