#include "CriticalLoadManager.h"
#include "Utils.h"

#include "llvm/Support/FileSystem.h"

class SliceTriggerInstruction : public DynamicInstruction {
public:
  SliceTriggerInstruction(const std::string &_SliceStreamFileName,
                          const llvm::Instruction *_CriticalInst)
      : SliceStreamFileName(_SliceStreamFileName), CriticalInst(_CriticalInst) {
  }
  std::string getOpName() const override { return "specpre-trigger"; }

  // There should be some customized fields in the future.
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    // Calling mutable_adfa_config should set this field.
    auto Extra = ProtobufEntry->mutable_specpre_trigger();
    assert(ProtobufEntry->has_specpre_trigger() &&
           "The protobuf entry should have specpre trigger extra struct.");
    Extra->set_slice_stream(this->SliceStreamFileName);
    Extra->set_critical_pc(Utils::getInstUIDMap().getUID(this->CriticalInst));
  }

private:
  std::string SliceStreamFileName;
  const llvm::Instruction *CriticalInst;
};

CriticalLoadManager::CriticalLoadManager(llvm::LoadInst *_CriticalLoad,
                                         const std::string &_RootPath)
    : CriticalLoad(_CriticalLoad), Status(StatusE::Unknown), HitsCount(0),
      SlicesCreated(0), SlicesTriggered(0), CorrectSlicesTriggered(0) {
  this->AnalyzePath =
      _RootPath + "/" + Utils::formatLLVMInst(this->CriticalLoad);
  auto ErrCode = llvm::sys::fs::create_directory(this->AnalyzePath);
  if (ErrCode) {
    llvm::errs() << "Failed to create AnalyzePath: " << this->AnalyzePath
                 << ". Reason: " << ErrCode.message() << '\n';
  }
  assert(!ErrCode && "Failed to create AnalyzePath.");

  this->SliceStream = this->AnalyzePath + "/slice.tdg";
  this->SliceStreamRelativePath =
      Utils::formatLLVMInst(this->CriticalLoad) + "/slice.tdg";

  bool DebugText = false;
  this->SliceSerializer =
      std::make_unique<TDGSerializer>(this->SliceStream, DebugText);
  // Serialize an empty static information to conform with format.
  LLVM::TDG::StaticInformation StaticInfo;
  /**
   * Debug try: Randomly fill something in static info.
   */
  StaticInfo.set_module("specpre");
  this->SliceSerializer->serializeStaticInfo(StaticInfo);
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

  auto SliceHeaderId = Slice->getHeaderDynamicId();
  if (this->Status == StatusE::Building) {
    this->SlicesCreated++;
    this->ComputedSlice = std::move(Slice);
    this->Status = StatusE::Built;
  } else {
    /**
     * Check if the slice is correct.
     */
    this->SlicesTriggered++;
    if (this->ComputedSlice->isSame(*Slice)) {
      this->CorrectSlicesTriggered++;
      Slice->generateSlice(this->SliceSerializer.get(), true);
    } else {
      this->ComputedSlice->generateSlice(this->SliceSerializer.get(), false);
    }
    // Insert the trigger instruction into the DG before the first inst of the
    // slice.
    auto TriggerInst = new SliceTriggerInstruction(
        this->SliceStreamRelativePath, this->CriticalLoad);
    auto SliceHead = DG->getDynamicInstFromId(SliceHeaderId);
    DG->insertDynamicInst(SliceHead, TriggerInst);
  }
}

void CriticalLoadManager::clear() {
  this->Status = StatusE::Unknown;
  this->ComputedSlice = nullptr;
}

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