#ifndef LLVM_TDG_STREAM_MEM_STREAM_H
#define LLVM_TDG_STREAM_MEM_STREAM_H

#include "MemoryFootprint.h"
#include "stream/Stream.h"

class MemStream : public Stream {
public:
  MemStream(const std::string &_Folder, const llvm::Instruction *_Inst,
            const llvm::Loop *_Loop, size_t _LoopLevel,
            std::function<bool(llvm::PHINode *)> IsInductionVar)
      : Stream(TypeT::MEM, _Folder, _Inst, _Loop, _LoopLevel) {
    assert(Utils::isMemAccessInst(this->Inst) &&
           "Should be load/store instruction to build a stream.");
    this->searchAddressComputeInstructions(IsInductionVar);
  }

  void addAccess(uint64_t Addr, DynamicInstruction::DynamicId DynamicId) {
    if (this->StartId == DynamicInstruction::InvalidId) {
      this->StartId = DynamicId;
    }
    this->Footprint.access(Addr);
    this->LastAccessIters = this->Iters;
    this->Pattern.addAccess(Addr);
    this->TotalAccesses++;
  }

  void addMissingAccess() { this->Pattern.addMissingAccess(); }

  void addAliasInst(llvm::Instruction *AliasInst) {
    this->AliasInsts.insert(AliasInst);
  }

  void finalizePattern() { this->Pattern.finalizePattern(); }

  const MemoryFootprint &getFootprint() const { return this->Footprint; }
  bool getQualified() const { return this->Qualified; }
  DynamicInstruction::DynamicId getStartId() const { return this->StartId; }

  bool isIndirect() const { return !this->BaseLoads.empty(); }
  size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
  size_t getNumAddrInsts() const { return this->AddrInsts.size(); }
  const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const override {
    return this->AddrInsts;
  }
  const std::unordered_set<llvm::LoadInst *> &getBaseLoads() const {
    return this->BaseLoads;
  }
  const std::unordered_set<llvm::PHINode *> &getBaseInductionVars() const {
    return this->BaseInductionVars;
  }
  const std::unordered_set<llvm::Instruction *> &getAliasInsts() const {
    return this->AliasInsts;
  }
  bool isAliased() const override {
    // Check if we have alias with *other* instructions.
    for (const auto &AliasInst : this->AliasInsts) {
      if (AliasInst != this->Inst) {
        return true;
      }
    }
    return false;
  }

private:
  MemoryFootprint Footprint;
  std::unordered_set<llvm::LoadInst *> BaseLoads;
  std::unordered_set<llvm::PHINode *> BaseInductionVars;
  std::unordered_set<const llvm::Instruction *> AddrInsts;
  std::unordered_set<llvm::Instruction *> AliasInsts;

  void searchAddressComputeInstructions(
      std::function<bool(llvm::PHINode *)> IsInductionVar);
};
#endif