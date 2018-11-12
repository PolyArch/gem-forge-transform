#include "DynamicStreamCoalescer.h"

DynamicStreamCoalescer::DynamicStreamCoalescer(FunctionalStream *_RootStream)
    : RootStream(_RootStream), TotalSteps(0) {

  assert(this->RootStream->isStepRoot() &&
         "Should only try to coalesce for step root streams.");

  const auto &AllDependentStepStreams =
      this->RootStream->getAllDependentStepStreamsSorted();

  // Initialize the Id map.
  for (const auto &DependentStepStream : AllDependentStepStreams) {

    // Let's skip those indirect memory stream.
    if (!DynamicStreamCoalescer::isDirectMemStream(DependentStepStream)) {
      continue;
    }

    auto FSId = this->FSIdMap.size();
    this->FSIdMap.emplace(DependentStepStream, FSId);
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

  const float Threshold = 0.9;

  /**
   * Standard union-find to coalesce streams.
   */

  for (auto &Row : this->FSIdMap) {
    for (auto &Col : this->FSIdMap) {
      auto CoalescedCount = this->CoalescerMatrix[Row.second][Col.second];
      // llvm::errs() << "Total " << this->TotalSteps << " Coalesced "
      //              << CoalescedCount << '\n';
      auto CoalescedPercentage = static_cast<float>(CoalescedCount) /
                                 static_cast<float>(this->TotalSteps);
      if (CoalescedPercentage > Threshold) {
        this->coalesce(Row.second, Col.second);
      }
    }
  }
}

int DynamicStreamCoalescer::getCoalesceGroup(FunctionalStream *FS) const {
  auto FSIdMapIter = this->FSIdMap.find(FS);
  if (FSIdMapIter == this->FSIdMap.end()) {
    return -1;
  } else {
    return this->findRoot(FSIdMapIter->second);
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
  if (S->Type != Stream::TypeT::MEM) {
    return false;
  }
  for (auto BaseS : S->getChosenBaseStepStreams()) {
    if (BaseS->Type != Stream::TypeT::IV) {
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