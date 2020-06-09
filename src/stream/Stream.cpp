#include "stream/Stream.h"

#include "llvm/Support/FileSystem.h"

#include "google/protobuf/util/json_util.h"

Stream::Stream(const std::string &_Folder, const std::string &_RelativeFolder,
               const StaticStream *_SStream, llvm::DataLayout *DataLayout)
    : SStream(_SStream), Folder(_Folder), RelativeFolder(_RelativeFolder),
      HasMissingBaseStream(false), Qualified(false), Chosen(false),
      RegionStreamId(-1), TotalIters(0), TotalAccesses(0), TotalStreams(0),
      Iters(1), LastAccessIters(0), StartId(DynamicInstruction::InvalidId),
      Pattern() {
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

  auto PosInBB = LoopUtils::getLLVMInstPosInBB(this->getInst());
}

void Stream::addBaseStream(Stream *Other) {
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

void Stream::addBackEdgeBaseStream(Stream *Other) {
  this->BackMemBaseStreams.insert(Other);
  Other->BackIVDependentStreams.insert(this);
}

/**
 * This must happen after all the calls to addBaseStream.
 */
void Stream::computeBaseStepRootStreams() {
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

void Stream::buildChosenDependenceGraph(GetChosenStreamFuncT GetChosenStream) {
  auto TranslateBasicToChosen =
      [this, &GetChosenStream](const StreamSet &BasicSet,
                               StreamSet &ChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      const auto &BaseInst = BaseS->SStream->Inst;
      auto ChosenBaseS = GetChosenStream(BaseInst);
      if (!ChosenBaseS) {
        llvm::errs() << "Miss chosen stream " << BaseS->SStream->formatName()
                     << " for " << this->SStream->formatName() << ".\n";
      }
      assert(ChosenBaseS && "Missing chosen base stream.");
      ChosenSet.insert(ChosenBaseS);
    }
  };
  auto TranslateBasicToChosenNullable =
      [this, &GetChosenStream](const StreamSet &BasicSet,
                               StreamSet &ChosenSet) -> void {
    for (const auto &BaseS : BasicSet) {
      const auto &BaseInst = BaseS->SStream->Inst;
      auto ChosenBaseS = GetChosenStream(BaseInst);
      if (!ChosenBaseS) {
        continue;
      }
      ChosenSet.insert(ChosenBaseS);
    }
  };
  // Also for the other types.
  TranslateBasicToChosen(this->BaseStreams, this->ChosenBaseStreams);
  // DependentStream may not be chosen
  TranslateBasicToChosenNullable(this->DependentStreams,
                                 this->ChosenDependentStreams);
  TranslateBasicToChosen(this->BackMemBaseStreams,
                         this->ChosenBackMemBaseStreams);
  // BackIVDependentStream may not be chosen
  TranslateBasicToChosenNullable(this->BackIVDependentStreams,
                                 this->ChosenBackIVDependentStreams);
  TranslateBasicToChosen(this->BaseStepStreams, this->ChosenBaseStepStreams);
  TranslateBasicToChosen(this->BaseStepRootStreams,
                         this->ChosenBaseStepRootStreams);
}

void Stream::endStream() {
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

void Stream::finalizePattern() {
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

void Stream::finalizeInfo(llvm::DataLayout *DataLayout) {
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

void Stream::fillProtobufStreamInfo(llvm::DataLayout *DataLayout,
                                    LLVM::TDG::StreamInfo *ProtobufInfo) const {
  auto ProtobufStaticInfo = ProtobufInfo->mutable_static_info();
  this->SStream->setStaticStreamInfo(*ProtobufStaticInfo);
  ProtobufInfo->set_name(this->formatName());
  ProtobufInfo->set_id(this->getStreamId());
  ProtobufInfo->set_region_stream_id(this->getRegionStreamId());
  switch (this->getInst()->getOpcode()) {
  case llvm::Instruction::PHI:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_IV);
    break;
  case llvm::Instruction::Load:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_LD);
    break;
  case llvm::Instruction::Store:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_ST);
    break;
  case llvm::Instruction::AtomicRMW:
  case llvm::Instruction::AtomicCmpXchg:
    ProtobufInfo->set_type(::LLVM::TDG::StreamInfo_Type_AT);
    break;
  default:
    llvm::errs() << "Invalid stream type " << this->formatName() << '\n';
    break;
  }
  ProtobufInfo->set_loop_level(this->getInnerMostLoop()->getLoopDepth());
  ProtobufInfo->set_config_loop_level(this->getLoop()->getLoopDepth());
  ProtobufInfo->set_pattern_path(this->getPatternRelativePath());
  ProtobufInfo->set_history_path(this->getHistoryRelativePath());

  // Dump the address function.
  auto AddrFuncInfo = ProtobufInfo->mutable_addr_func_info();
  this->fillProtobufAddrFuncInfo(DataLayout, AddrFuncInfo);
  // Dump the predication function.
  auto PredFuncInfo = ProtobufStaticInfo->mutable_pred_func_info();
  this->fillProtobufPredFuncInfo(DataLayout, PredFuncInfo);
  // Dump the store function.
  this->fillProtobufStoreFuncInfo(DataLayout, ProtobufStaticInfo);

  auto ProtobufCoalesceInfo = ProtobufInfo->mutable_coalesce_info();
  ProtobufCoalesceInfo->set_base_stream(this->CoalesceGroup);
  ProtobufCoalesceInfo->set_offset(this->CoalesceOffset);

  auto DynamicStreamInfo = ProtobufInfo->mutable_dynamic_info();
  DynamicStreamInfo->set_is_candidate(this->isCandidate());
  DynamicStreamInfo->set_is_qualified(this->isQualified());
  DynamicStreamInfo->set_is_chosen(this->isChosen());
  DynamicStreamInfo->set_is_aliased(this->isAliased());
  DynamicStreamInfo->set_total_iters(this->TotalIters);
  DynamicStreamInfo->set_total_accesses(this->TotalAccesses);
  DynamicStreamInfo->set_total_configures(this->TotalStreams);

#define ADD_STREAM(SET, FIELD)                                                 \
  {                                                                            \
    for (const auto &S : SET) {                                                \
      auto Entry = ProtobufInfo->add_##FIELD();                                \
      Entry->set_name(S->formatName());                                        \
      Entry->set_id(S->getStreamId());                                         \
    }                                                                          \
  }
  ADD_STREAM(this->BaseStreams, base_streams);
  ADD_STREAM(this->BackMemBaseStreams, back_base_streams);
  ADD_STREAM(this->ChosenBaseStreams, chosen_base_streams);
  ADD_STREAM(this->ChosenBackMemBaseStreams, chosen_back_base_streams);

#undef ADD_STREAM
}

const Stream *Stream::getExecFuncInputStream(const llvm::Value *Value) const {
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

Stream::InputValueList
Stream::getExecFuncInputValues(const ExecutionDataGraph &ExecDG) const {
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

void Stream::fillProtobufExecFuncInfo(::llvm::DataLayout *DataLayout,
                                      ::LLVM::TDG::ExecFuncInfo *ProtoFuncInfo,
                                      const std::string &FuncName,
                                      const ExecutionDataGraph &ExecDG) const {

  ProtoFuncInfo->set_name(FuncName);
  for (const auto &Input : ExecDG.getInputs()) {
    auto ProtobufArg = ProtoFuncInfo->add_args();
    auto Type = Input->getType();
    // if (!Type->isIntOrPtrTy()) {
    //   llvm::errs() << "Invalid type, Value: " <<
    //   Utils::formatLLVMValue(Input)
    //                << '\n';
    //   assert(false && "Invalid type for input.");
    // }
    if (auto InputStream = this->getExecFuncInputStream(Input)) {
      // This comes from the base stream.
      ProtobufArg->set_is_stream(true);
      ProtobufArg->set_stream_id(InputStream->getStreamId());
    } else {
      // This is an input value.
      ProtobufArg->set_is_stream(false);
    }
    ProtobufArg->set_is_float(false);
  }
  ProtoFuncInfo->set_is_float(false);
}
