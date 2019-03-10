#include "CriticalLoadManager.h"
#include "Utils.h"

#include "llvm/Support/FileSystem.h"

CriticalLoadManager::CriticalLoadManager(llvm::LoadInst *_CriticalLoad,
                                         const std::string &_RootPath)
    : CriticalLoad(_CriticalLoad),
      Status(StatusE::Unknown),
      HitsCount(0),
      SlicesCreated(0),
      SlicesTriggered(0),
      CorrectSlicesTriggered(0) {
  this->AnalyzePath =
      _RootPath + "/" + Utils::formatLLVMInst(this->CriticalLoad);
  auto ErrCode = llvm::sys::fs::create_directory(this->AnalyzePath);
  assert(!ErrCode && "Failed to create AnalyzePath.");
}

void CriticalLoadManager::hit(DataGraph *DG) {
  this->HitsCount++;

  if (this->Status == StatusE::Unknown) {
    this->Status = StatusE::Idle;
    return;
  }

  if (this->Status == StatusE::Idle) {
    this->Status = StatusE::Building;
    return;
  }

  // We first build the slice.
  auto Slice = std::make_unique<PrecomputationSlice>(this->CriticalLoad, DG);

  if (this->Status == StatusE::Building) {
    this->SlicesCreated++;
    this->ComputedSlice = std::move(Slice);
    this->Status = StatusE::Built;
  } else {
    /**
     * Check if the slice is correct.
     */
    this->SlicesTriggered++;
    this->CorrectSlicesTriggered++;
  }
}

void CriticalLoadManager::clear() { this->Status = StatusE::Unknown; }

void CriticalLoadManager::dump() const {
  std::ofstream InfoFStream(this->AnalyzePath + "/info.txt");
  assert(InfoFStream.is_open() && "Failed to open critical load info file.");

#define DUMP(field) InfoFStream << #field << ' ' << this->field << '\n';

  DUMP(HitsCount);
  DUMP(SlicesCreated);
  DUMP(SlicesTriggered);
  DUMP(CorrectSlicesTriggered);

#undef DUMP

  InfoFStream.close();
}