#include "DynamicStreamCoalescer.h"

int DynamicStreamCoalescer::AllocatedGlobalCoalesceGroup = 0;

int DynamicStreamCoalescer::allocateGlobalCoalesceGroup() {
  // Make the coalesce group starts from 1.
  return ++AllocatedGlobalCoalesceGroup;
}

DynamicStreamCoalescer::DynamicStreamCoalescer(
    const std::unordered_set<FunctionalStream *> &FuncStreams)
    : TotalSteps(0) {
  // Initialize the Id map.
  for (const auto &FS : FuncStreams) {
    // Let's skip those indirect memory stream.
    assert(DynamicStreamCoalescer::isDirectMemStream(FS) &&
           "Only coalescing for direct memory stream.");

    auto FSId = this->FSIdMap.size();
    this->FSIdMap.emplace(FS, FSId);
  }

  // Initialize the coalesce matrix and the UFArray.
  this->CoalescerMatrix.reserve(this->FSIdMap.size());
  this->UFArray.reserve(this->FSIdMap.size());
  for (int Id = 0; Id < this->FSIdMap.size(); ++Id) {
    this->CoalescerMatrix.emplace_back(this->FSIdMap.size(), 0);
    this->UFArray.push_back(Id);
  }
}

DynamicStreamCoalescer::~DynamicStreamCoalescer() {}

namespace {
const size_t CacheLineSize = 64;
const size_t CacheLineDiff = 1;
} // namespace
#define CacheLineAddr(addr) (addr & (~(64 - 1)))

void DynamicStreamCoalescer::updateCoalesceMatrix() {
  this->TotalSteps++;
  for (auto &Row : this->FSIdMap) {
    auto XAddr = Row.first->getAddress();
    auto XCacheLineAddr = CacheLineAddr(XAddr);
    // std::cerr << "XAddr " << std::hex << XAddr << '\n';
    for (auto &Col : this->FSIdMap) {
      auto YAddr = Col.first->getAddress();
      auto YCacheLineAddr = CacheLineAddr(YAddr);
      if (std::abs(static_cast<int64_t>(YCacheLineAddr) -
                   static_cast<int64_t>(XCacheLineAddr)) <=
          CacheLineDiff * CacheLineSize) {
        this->CoalescerMatrix[Row.second][Col.second]++;
      }
    }
  }
}

void DynamicStreamCoalescer::finalize() {
  if (this->TotalSteps == 0) {
    return;
  }

  const float Threshold = 0.95;

  /**
   * Standard union-find to coalesce streams.
   */

  for (auto &Row : this->FSIdMap) {
    for (auto &Col : this->FSIdMap) {
      auto CoalescedCount = this->CoalescerMatrix[Row.second][Col.second];
      // llvm::errs() << "Total " << this->TotalSteps << " Coalesced "
      //              << CoalescedCount << ' '
      //              << Row.first->getStream()->formatName() << ' '
      //              << Col.first->getStream()->formatName() << '\n';
      auto CoalescedPercentage = static_cast<float>(CoalescedCount) /
                                 static_cast<float>(this->TotalSteps);
      auto RowStream = Row.first->getStream();
      auto ColStream = Col.first->getStream();
      /**
       * ! Only coalesce when they are the same type of streams with high
       * ! coalesce percentage.
       */
      if (CoalescedPercentage > Threshold &&
          (RowStream->SStream->Inst->getOpcode() ==
           ColStream->SStream->Inst->getOpcode())) {
        // llvm::errs() << "Coalescing " << Row.first->getStream()->formatName()
        //              << ' ' << Col.first->getStream()->formatName() << '\n';
        this->coalesce(Row.second, Col.second);
      }
    }
  }

  // Allocate all the global coalesce group id.
  for (auto &FSId : this->FSIdMap) {
    auto LocalGroup = this->findRoot(FSId.second);
    if (this->LocalToGlobalCoalesceGroupMap.count(LocalGroup) == 0) {
      this->LocalToGlobalCoalesceGroupMap.emplace(
          LocalGroup, DynamicStreamCoalescer::allocateGlobalCoalesceGroup());
    }
    auto GlobalGroup = this->LocalToGlobalCoalesceGroupMap.at(LocalGroup);
    FSId.first->getStream()->setCoalesceGroup(GlobalGroup);
    // llvm::errs() << "Setting Coalesce group " << GlobalGroup << " of "
    //              << FSId.first->getStream()->formatName() << '\n';
  }
}

void DynamicStreamCoalescer::coalesce(int A, int B) {
  // Without size balancing.
  auto RootA = this->findRoot(A);
  auto RootB = this->findRoot(B);
  if (RootA != RootB) {
    this->UFArray[RootA] = RootB;
  }
}

int DynamicStreamCoalescer::findRoot(int A) {
  assert(A >= 0 && "Negative Idx.");
  assert(A < this->UFArray.size() && "Overflow Idx.");
  if (this->UFArray[A] == A) {
    return A;
  } else {
    auto Root = this->findRoot(this->UFArray[A]);
    // Depth compression.
    this->UFArray[A] = Root;
    return Root;
  }
}

int DynamicStreamCoalescer::findRoot(int A) const {
  assert(A >= 0 && "Negative Idx.");
  assert(A < this->UFArray.size() && "Overflow Idx.");
  if (this->UFArray[A] == A) {
    return A;
  } else {
    auto Root = this->findRoot(this->UFArray[A]);
    return Root;
  }
}

bool DynamicStreamCoalescer::isDirectMemStream(FunctionalStream *FuncS) {
  auto S = FuncS->getStream();
  if (S->SStream->Type != StaticStream::TypeT::MEM) {
    return false;
  }
  for (auto BaseS : S->getChosenBaseStepStreams()) {
    if (BaseS->SStream->Type != StaticStream::TypeT::IV) {
      // Base on a mem stream.
      if (BaseS->getChosenBaseStepStreams().empty()) {
        // This is a constant load. Ignore it.
        continue;
      }
      // Indirect stream.
      return false;
    }
  }
  return true;
}