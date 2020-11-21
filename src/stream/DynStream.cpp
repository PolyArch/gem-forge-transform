#include "stream/DynStream.h"

#include "llvm/Support/FileSystem.h"

#include "google/protobuf/util/json_util.h"

DynStream::DynStream(const std::string &_Folder,
                     const std::string &_RelativeFolder, StaticStream *_SStream,
                     llvm::DataLayout *DataLayout)
    : SStream(_SStream), Folder(_Folder), RelativeFolder(_RelativeFolder),
      HasMissingBaseStream(false), Qualified(false), TotalIters(0),
      TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
      StartId(DynamicInstruction::InvalidId), Pattern() {
  auto PatternFolder = this->Folder + "/pattern";
  auto ErrCode = llvm::sys::fs::create_directory(PatternFolder);
  assert(!ErrCode && "Failed to create pattern folder.");

  auto HistoryFolder = this->Folder + "/history";
  ErrCode = llvm::sys::fs::create_directory(HistoryFolder);
  assert(!ErrCode && "Failed to create history folder.");

  auto InfoFolder = this->Folder + "/info";
  ErrCode = llvm::sys::fs::create_directory(InfoFolder);
  assert(!ErrCode && "Failed to create info folder.");

  this->PatternFileName = "pattern/" + this->formatName() + ".pattern";
  this->InfoFileName = "info/" + this->formatName() + ".info";
  this->HistoryFileName = "history/" + this->formatName() + ".history";

  auto PosInBB = Utils::getLLVMInstPosInBB(this->getInst());
}

void DynStream::addBaseStream(DynStream *Other) {
  // assert(Other != this && "Self dependent streams is not allowed.");
  this->BaseStreams.insert(Other);
  if (Other != nullptr) {
    Other->DependentStreams.insert(this);

    if (Other->getInnerMostLoop() == this->getInnerMostLoop()) {
      // We are in the same loop level. This is also a step stream for me.
      this->BaseStepStreams.insert(Other);
    }

  } else {
    this->HasMissingBaseStream = true;
  }
}

void DynStream::addBackEdgeBaseStream(DynStream *Other) {
  this->BackMemBaseStreams.insert(Other);
  Other->BackIVDependentStreams.insert(this);
}

/**
 * This must happen after all the calls to addBaseStream.
 */
void DynStream::computeBaseStepRootStreams() {
  for (auto &BaseStepStream : this->BaseStepStreams) {
    if (BaseStepStream->SStream->Type == StaticStream::TypeT::IV) {
      // Induction variable is always a root stream.
      this->BaseStepRootStreams.insert(BaseStepStream);
      // No need to go deeper for IVStream.
      continue;
    }
    if (BaseStepStream->getBaseStepRootStreams().empty()) {
      // If this is empty, recompute (even if recomputed).
      BaseStepStream->computeBaseStepRootStreams();
    }
    for (auto &BaseStepRootStream : BaseStepStream->getBaseStepRootStreams()) {
      this->BaseStepRootStreams.insert(BaseStepRootStream);
    }
  }
}

void DynStream::buildChosenDependenceGraph(
    GetChosenStreamFuncT GetChosenStream) {
  auto TranslateBasicToChosen =
      [this,
       &GetChosenStream](const StreamSet &BasicSet, StreamSet &ChosenSet,
                         StaticStream::StreamSet &StaticChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      const auto &BaseInst = BaseS->SStream->Inst;
      auto ChosenBaseS = GetChosenStream(BaseInst);
      if (!ChosenBaseS) {
        llvm::errs() << "Miss chosen stream " << BaseS->SStream->formatName()
                     << " for " << this->SStream->formatName() << ".\n";
      }
      assert(ChosenBaseS && "Missing chosen base stream.");
      ChosenSet.insert(ChosenBaseS);
      StaticChosenSet.insert(ChosenBaseS->SStream);
    }
  };
  auto TranslateBasicToChosenNullable =
      [this,
       &GetChosenStream](const StreamSet &BasicSet, StreamSet &ChosenSet,
                         StaticStream::StreamSet &StaticChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      const auto &BaseInst = BaseS->SStream->Inst;
      auto ChosenBaseS = GetChosenStream(BaseInst);
      if (!ChosenBaseS) {
        continue;
      }
      ChosenSet.insert(ChosenBaseS);
      StaticChosenSet.insert(ChosenBaseS->SStream);
    }
  };
  // Also for the other types.
  TranslateBasicToChosen(this->BaseStreams, this->ChosenBaseStreams,
                         this->SStream->ChosenBaseStreams);
  // DependentStream may not be chosen
  TranslateBasicToChosenNullable(this->DependentStreams,
                                 this->ChosenDependentStreams,
                                 this->SStream->ChosenDependentStreams);
  TranslateBasicToChosen(this->BackMemBaseStreams,
                         this->ChosenBackMemBaseStreams,
                         this->SStream->ChosenBackMemBaseStreams);
  // BackIVDependentStream may not be chosen
  TranslateBasicToChosenNullable(this->BackIVDependentStreams,
                                 this->ChosenBackIVDependentStreams,
                                 this->SStream->ChosenBackIVDependentStreams);
  TranslateBasicToChosen(this->BaseStepStreams, this->ChosenBaseStepStreams,
                         this->SStream->ChosenBaseStepStreams);
  TranslateBasicToChosen(this->BaseStepRootStreams,
                         this->ChosenBaseStepRootStreams,
                         this->SStream->ChosenBaseStepRootStreams);
}

void DynStream::endStream() {
  const auto ComputedPattern = this->Pattern.endStream();
  this->Iters = 1;
  this->LastAccessIters = 0;
  this->TotalStreams++;
  this->StartId = DynamicInstruction::InvalidId;

  this->ProtobufPatterns.emplace_back();
  auto &ProtobufPattern = this->ProtobufPatterns.back();

  ProtobufPattern.set_val_pattern(
      StreamPattern::formatValuePattern(ComputedPattern.ValPattern));
  ProtobufPattern.set_acc_pattern(
      StreamPattern::formatAccessPattern(ComputedPattern.AccPattern));
  ProtobufPattern.set_iters(ComputedPattern.Iters);
  ProtobufPattern.set_accesses(ComputedPattern.Accesses);
  ProtobufPattern.set_updates(ComputedPattern.Updates);
  ProtobufPattern.set_base(ComputedPattern.Base);
  ProtobufPattern.set_stride_i(ComputedPattern.StrideI);
  ProtobufPattern.set_ni(ComputedPattern.NI);
  ProtobufPattern.set_stride_j(ComputedPattern.StrideJ);
  ProtobufPattern.clear_history();
  for (const auto &HistoryEntry : ComputedPattern.History) {
    auto ProtobufHistoryEntry = ProtobufPattern.add_history();
    ProtobufHistoryEntry->set_valid(HistoryEntry.first);
    if (HistoryEntry.first) {
      ProtobufHistoryEntry->set_value(HistoryEntry.second);
    }
  }
}

void DynStream::finalizePattern() {
  this->Pattern.finalizePattern();

  Gem5ProtobufSerializer PatternSerializer(this->getPatternFullPath());
  LLVM::TDG::StreamInfo ProtobufInfo;

  std::ofstream PatternTextFStream(
      this->getTextPath(this->getPatternFullPath()));
  assert(PatternTextFStream.is_open() && "Failed to open output stream file.");

  for (auto &ProtobufPattern : this->ProtobufPatterns) {
    PatternSerializer.serialize(ProtobufPattern);
    std::string PatternJsonString;
    // For the json file, we do not log history.
    ProtobufPattern.clear_history();
    google::protobuf::util::MessageToJsonString(ProtobufPattern,
                                                &PatternJsonString);
    PatternTextFStream << PatternJsonString << '\n';
  }

  PatternTextFStream.close();
}

void DynStream::finalizeInfo(llvm::DataLayout *DataLayout) {
  // Also serialize with protobuf.
  Gem5ProtobufSerializer InfoSerializer(this->getInfoFullPath());
  LLVM::TDG::StreamInfo ProtobufInfo;
  this->fillProtobufStreamInfo(DataLayout, &ProtobufInfo);
  InfoSerializer.serialize(ProtobufInfo);

  std::ofstream InfoTextFStream(this->getTextPath(this->getInfoFullPath()));
  assert(InfoTextFStream.is_open() && "Failed to open the output info file.");
  std::string InfoJsonString;
  google::protobuf::util::MessageToJsonString(ProtobufInfo, &InfoJsonString);
  InfoTextFStream << InfoJsonString << '\n';
  InfoTextFStream.close();
}

void DynStream::fillProtobufStreamInfo(
    llvm::DataLayout *DataLayout, LLVM::TDG::StreamInfo *ProtobufInfo) const {
  this->SStream->fillProtobufStreamInfo(ProtobufInfo);
  ProtobufInfo->set_pattern_path(this->getPatternRelativePath());
  ProtobufInfo->set_history_path(this->getHistoryRelativePath());

  auto DynamicStreamInfo = ProtobufInfo->mutable_dynamic_info();
  DynamicStreamInfo->set_is_aliased(this->isAliased());
  DynamicStreamInfo->set_total_iters(this->TotalIters);
  DynamicStreamInfo->set_total_accesses(this->TotalAccesses);
  DynamicStreamInfo->set_total_configures(this->TotalStreams);
}

const DynStream *
DynStream::getExecFuncInputStream(const llvm::Value *Value) const {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    if (Inst == this->SStream->Inst) {
      // The input is myself. Only for PredFunc.
      return this;
    }
    for (auto BaseStream : this->getChosenBaseStreams()) {
      if (BaseStream->SStream->Inst == Inst) {
        return BaseStream;
      }
    }
    if (this->SStream->ReduceDG) {
      for (auto BackBaseStream : this->ChosenBackMemBaseStreams) {
        if (BackBaseStream->SStream->Inst == Inst) {
          // The input is back base stream. Only for ReduceFunc.
          return BackBaseStream;
        }
        // Search for the induction variable of the BackBaseStream.
        for (auto BackBaseStepRootStream :
             BackBaseStream->ChosenBaseStepRootStreams) {
          if (BackBaseStepRootStream->SStream->Inst == Inst) {
            return BackBaseStepRootStream;
          }
        }
      }
    }
  }
  return nullptr;
}

DynStream::InputValueList
DynStream::getExecFuncInputValues(const ExecutionDataGraph &ExecDG) const {
  InputValueList InputValues;
  for (const auto &Input : ExecDG.getInputs()) {
    if (auto InputStream = this->getExecFuncInputStream(Input)) {
      // This comes from the base stream.
    } else {
      // This is an input value.
      InputValues.push_back(Input);
    }
  }
  return InputValues;
}
