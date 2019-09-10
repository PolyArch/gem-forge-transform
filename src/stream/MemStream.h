#ifndef LLVM_TDG_STREAM_MEM_STREAM_H
#define LLVM_TDG_STREAM_MEM_STREAM_H

#include "MemoryFootprint.h"
#include "stream/Stream.h"
#include "stream/ae/AddressDataGraph.h"

extern llvm::cl::opt<bool> StreamPassAllowAliasedStream;

class MemStream : public Stream {
public:
  MemStream(const std::string &_Folder, const std::string &_RelativeFolder,
            const StaticStream *_SStream, llvm::DataLayout *DataLayout,
            std::function<bool(const llvm::PHINode *)> IsInductionVar)
      : Stream(_Folder, _RelativeFolder, _SStream, DataLayout),
        AddrDG(_SStream->ConfigureLoop, Utils::getMemAddrValue(_SStream->Inst),
               IsInductionVar) {
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
  const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const override {
    return this->AddrInsts;
  }
  const std::unordered_set<const llvm::LoadInst *> &
  getBaseLoads() const override {
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
      if (AliasInst != this->getInst()) {
        return true;
      }
    }
    return false;
  }

  std::string getAddressFunctionName() const {
    return this->AddressFunctionName;
  }

  bool isCandidate() const override;
  bool isQualifySeed() const override;

  const AddressDataGraph &getAddressDataGraph() const { return this->AddrDG; }

  void generateComputeFunction(std::unique_ptr<llvm::Module> &Module) const;

protected:
  void formatAdditionalInfoText(std::ostream &OStream) const override;

private:
  MemoryFootprint Footprint;
  std::unordered_set<const llvm::LoadInst *> BaseLoads;
  std::unordered_set<llvm::PHINode *> BaseInductionVars;
  std::unordered_set<const llvm::Instruction *> AddrInsts;
  std::unordered_set<llvm::Instruction *> AliasInsts;

  AddressDataGraph AddrDG;

  void searchAddressComputeInstructions(
      std::function<bool(const llvm::PHINode *)> IsInductionVar);
};
#endif