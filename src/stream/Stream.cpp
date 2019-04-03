#include "stream/Stream.h"

#include "llvm/Support/FileSystem.h"

#include "google/protobuf/util/json_util.h"

Stream::Stream(const std::string &_Folder, const std::string &_RelativeFolder,
               const StaticStream *_SStream, llvm::DataLayout *DataLayout)
    : SStream(_SStream), Folder(_Folder), RelativeFolder(_RelativeFolder),
      HasMissingBaseStream(false), Qualified(false), Chosen(false),
      IsStaticStream(false), CoalesceGroup(-1), TotalIters(0), TotalAccesses(0),
      TotalStreams(0), Iters(1), LastAccessIters(0),
      StartId(DynamicInstruction::InvalidId), Pattern() {
  this->ElementSize = this->getElementSize(DataLayout);

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

void Stream::addChosenBaseStream(Stream *Other) {
  assert(Other != this && "Self dependent chosen streams is not allowed.");
  assert(this->isChosen() &&
         "This should be chosen to build the chosen stream dependence graph.");
  assert(Other->isChosen() &&
         "Other should be chosen to build the chosen stream dependence graph.");
  this->ChosenBaseStreams.insert(Other);
  // Set the base stream as step stream if we share the same inner most level.
  if (Other->getInnerMostLoop() == this->getInnerMostLoop()) {
    this->ChosenBaseStepStreams.insert(Other);
    if (Other->SStream->Type == StaticStream::TypeT::IV) {
      this->ChosenBaseStepRootStreams.insert(Other);
    }
  }
}

void Stream::addChosenBackEdgeBaseStream(Stream *Other) {
  this->ChosenBackMemBaseStreams.insert(Other);
  Other->ChosenBackIVDependentStreams.insert(this);
}

void Stream::addAllChosenBaseStream(Stream *Other) {
  assert(Other != this && "Self dependent chosen streams is not allowed.");
  assert(this->isChosen() &&
         "This should be chosen to build the chosen stream dependence graph.");
  assert(Other->isChosen() &&
         "Other should be chosen to build the chosen stream dependence graph.");
  this->AllChosenBaseStreams.insert(Other);
  // Register myself at other's all chosen dependent streams.
  Other->AllChosenDependentStreams.insert(this);
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

  std::ofstream PatternTextFStream(
      this->getTextPath(this->getPatternFullPath()));
  assert(PatternTextFStream.is_open() && "Failed to open output stream file.");

  for (auto &ProtobufPattern : this->ProtobufPatterns) {
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
  ProtobufInfo->set_name(this->formatName());
  ProtobufInfo->set_id(this->getStreamId());
  ProtobufInfo->set_type(this->SStream->Inst->getOpcodeName());
  ProtobufInfo->set_loop_level(this->getInnerMostLoop()->getLoopDepth());
  ProtobufInfo->set_config_loop_level(this->getLoop()->getLoopDepth());
  ProtobufInfo->set_element_size(this->ElementSize);
  ProtobufInfo->set_pattern_path(this->getPatternRelativePath());
  ProtobufInfo->set_history_path(this->getHistoryRelativePath());
  ProtobufInfo->set_coalesce_group(this->CoalesceGroup);
  ProtobufInfo->set_chosen(this->Chosen);
  ProtobufInfo->set_is_static_stream(this->IsStaticStream);

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

#undef ADD_STREAM
}

int Stream::getElementSize(llvm::DataLayout *DataLayout) const {
  if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(this->getInst())) {
    llvm::Type *StoredType =
        StoreInst->getPointerOperandType()->getPointerElementType();
    return DataLayout->getTypeStoreSize(StoredType);
  }

  if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(this->getInst())) {
    llvm::Type *LoadedType =
        LoadInst->getPointerOperandType()->getPointerElementType();
    return DataLayout->getTypeStoreSize(LoadedType);
  }

  if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(this->getInst())) {
    return DataLayout->getTypeStoreSize(PHINode->getType());
  }

  llvm_unreachable("Unsupport stream type to get element size.");
}