#include "CriticalLoadManager.h"
#include "Utils.h"

#include "llvm/Support/FileSystem.h"

CriticalLoadManager::CriticalLoadManager(llvm::LoadInst *_CriticalLoad,
                                         const std::string &_RootPath)
    : CriticalLoad(_CriticalLoad), Status(StatusE::Unknown) {

  this->AnalyzePath =
      _RootPath + "/" + Utils::formatLLVMInst(this->CriticalLoad);
  auto ErrCode = llvm::sys::fs::create_directory(this->AnalyzePath);
  assert(!ErrCode && "Failed to create AnalyzePath.");
}

void CriticalLoadManager::hit(DataGraph *DG) {
  if (this->Status == StatusE::Unknown) {
    this->Status = StatusE::Idle;
    return;
  }

  if (this->Status == StatusE::Idle) {
    this->Status = StatusE::Building;
    return;
  }

  // We first build the slice.
}

void CriticalLoadManager::clear() { this->Status = StatusE::Unknown; }