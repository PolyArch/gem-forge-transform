#ifndef LLVM_TDG_STREAM_MEM_STREAM_H
#define LLVM_TDG_STREAM_MEM_STREAM_H

#include "MemoryFootprint.h"
#include "stream/DynStream.h"
#include "stream/ae/StreamDataGraph.h"

class DynMemStream : public DynStream {
public:
  using InstSet = StaticStream::InstSet;
  DynMemStream(const std::string &_Folder, const std::string &_RelativeFolder,
               StaticStream *_SStream, llvm::DataLayout *DataLayout,
               IsIndVarFunc IsInductionVar)
      : DynStream(_Folder, _RelativeFolder, _SStream, DataLayout) {
    assert(Utils::isMemAccessInst(this->getInst()) &&
           "Should be load/store instruction to build a stream.");
    this->searchAddressComputeInstructions(IsInductionVar);
  }

  void buildBasicDependenceGraph(GetStreamFuncT GetStream) override;

  void addAccess(uint64_t Addr,
                 DynamicInstruction::DynamicId DynamicId) override {
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

  const MemoryFootprint &getFootprint() const { return this->Footprint; }
  bool getQualified() const { return this->Qualified; }
  DynamicInstruction::DynamicId getStartId() const { return this->StartId; }

  bool isIndirect() const { return !this->BaseLoads.empty(); }
  size_t getNumBaseLoads() const { return this->BaseLoads.size(); }
  size_t getNumAddrInsts() const { return this->AddrInsts.size(); }
  const InstSet &getComputeInsts() const override { return this->AddrInsts; }
  const std::unordered_set<const llvm::LoadInst *> &
  getBaseLoads() const override {
    return this->BaseLoads;
  }
  const std::unordered_set<llvm::PHINode *> &getBaseInductionVars() const {
    return this->BaseInductionVars;
  }
  const InstSet &getAliasInsts() const { return this->AliasInsts; }
  bool isAliased() const override {
    // Check if we have alias with *other* instructions.
    for (const auto &AliasInst : this->AliasInsts) {
      if (AliasInst != this->getInst()) {
        return true;
      }
    }
    return false;
  }

  std::string getAddressFunctionName() const {
    return this->SStream->FuncNameBase + "_addr";
  }

  bool isCandidate() const override;
  bool isQualifySeed() const override;

  const StreamDataGraph &getAddrDG() const {
    assert(this->SStream->AddrDG && "Missing AddrDG.");
    return *this->SStream->AddrDG;
  }

  void generateFunction(std::unique_ptr<llvm::Module> &Module) const;

protected:
  void formatAdditionalInfoText(std::ostream &OStream) const override;

private:
  MemoryFootprint Footprint;
  std::unordered_set<const llvm::LoadInst *> BaseLoads;
  std::unordered_set<llvm::PHINode *> BaseInductionVars;
  InstSet AddrInsts;
  InstSet AliasInsts;

  void searchAddressComputeInstructions(
      std::function<bool(const llvm::PHINode *)> IsInductionVar);
};
#endif