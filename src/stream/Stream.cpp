#include "stream/Stream.h"

Stream::Stream(TypeT _Type, const std::string &_Folder,
               const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
               const llvm::Loop *_InnerMostLoop, size_t _LoopLevel)
    : Type(_Type), Folder(_Folder), Inst(_Inst), Loop(_Loop),
      InnerMostLoop(_InnerMostLoop), LoopLevel(_LoopLevel),
      HasMissingBaseStream(false), Qualified(false), Chosen(false),
      TotalIters(0), TotalAccesses(0), TotalStreams(0), Iters(1),
      LastAccessIters(0), StartId(DynamicInstruction::InvalidId), Pattern() {
  this->PatternFullPath = this->Folder + "/" + this->formatName() + ".pattern";
  this->PatternTextFullPath = this->PatternFullPath + ".txt";
  this->InfoFullPath = this->Folder + "/" + this->formatName() + ".info";
  this->InfoTextFullPath = this->InfoFullPath + ".txt";
  this->HistoryFullPath = this->Folder + "/" + this->formatName() + ".history";
  this->HistoryTextFullPath = this->HistoryFullPath + ".txt";
  this->PatternSerializer = new Gem5ProtobufSerializer(this->PatternFullPath);
  this->PatternTextFStream.open(this->PatternTextFullPath);
  assert(this->PatternTextFStream.is_open() &&
         "Failed to open output stream file.");
}

void Stream::addBaseStream(Stream *Other) {
  // assert(Other != this && "Self dependent streams is not allowed.");
  this->BaseStreams.insert(Other);
  if (Other != nullptr) {
    Other->DependentStreams.insert(this);

    if (Other->getInnerMostLoop() == this->InnerMostLoop) {
      // We are in the same loop level. This is also a step stream for me.
      this->BaseStepStreams.insert(Other);
    }

  } else {
    this->HasMissingBaseStream = true;
  }
}

/**
 * This must happen after all the calls to addBaseStream.
 */
void Stream::computeBaseStepRootStreams() {
  for (auto &BaseStepStream : this->BaseStepStreams) {
    if (BaseStepStream->Type == Stream::TypeT::IV) {
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
  if (Other->InnerMostLoop == this->InnerMostLoop) {
    this->ChosenBaseStepStreams.insert(Other);
  }
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

  // Serialize just with text.
  this->PatternTextFStream << "ValPat "
                           << StreamPattern::formatValuePattern(
                                  ComputedPattern.ValPattern)
                           << std::endl;
  this->PatternTextFStream << "AccPat "
                           << StreamPattern::formatAccessPattern(
                                  ComputedPattern.AccPattern)
                           << std::endl;
#define SerializeFieldText(field)                                              \
  this->PatternTextFStream << #field << ' ' << ComputedPattern.field           \
                           << std::endl
  SerializeFieldText(Iters);
  SerializeFieldText(Accesses);
  SerializeFieldText(Updates);
  SerializeFieldText(Base);
  SerializeFieldText(StrideI);
  SerializeFieldText(NI);
  SerializeFieldText(StrideJ);
#undef SerializeFieldText

  // Also serialize with protobuf.
  assert(this->PatternSerializer != nullptr &&
         "The pattern serializer has already been released.");
  this->ProtobufPattern.set_val_pattern(
      StreamPattern::formatValuePattern(ComputedPattern.ValPattern));
  this->ProtobufPattern.set_acc_pattern(
      StreamPattern::formatAccessPattern(ComputedPattern.AccPattern));
  this->ProtobufPattern.set_iters(ComputedPattern.Iters);
  this->ProtobufPattern.set_accesses(ComputedPattern.Accesses);
  this->ProtobufPattern.set_updates(ComputedPattern.Updates);
  this->ProtobufPattern.set_base(ComputedPattern.Base);
  this->ProtobufPattern.set_stride_i(ComputedPattern.StrideI);
  this->ProtobufPattern.set_ni(ComputedPattern.NI);
  this->ProtobufPattern.set_stride_j(ComputedPattern.StrideJ);
  this->ProtobufPattern.clear_history();
  for (const auto &HistoryEntry : ComputedPattern.History) {
    auto ProtobufHistoryEntry = this->ProtobufPattern.add_history();
    ProtobufHistoryEntry->set_valid(HistoryEntry.first);
    if (HistoryEntry.first) {
      ProtobufHistoryEntry->set_value(HistoryEntry.second);
    }
  }
  this->PatternSerializer->serialize(this->ProtobufPattern);
}

void Stream::finalize(llvm::DataLayout *DataLayout) {
  this->Pattern.finalizePattern();
  this->PatternTextFStream.close();
  delete this->PatternSerializer;
  this->PatternSerializer = nullptr;

  std::ofstream InfoTextFStream(this->InfoTextFullPath);
  assert(InfoTextFStream.is_open() && "Failed to open the output info file.");

  InfoTextFStream << this->formatName() << '\n';               // stream name
  InfoTextFStream << this->getStreamId() << '\n';              // stream id
  InfoTextFStream << this->Inst->getOpcodeName() << '\n';      // stream type
  InfoTextFStream << this->getElementSize(DataLayout) << '\n'; // element size
  InfoTextFStream << this->PatternFullPath << '\n';            // pattern path
  InfoTextFStream << this->HistoryFullPath << '\n';            // history path

  // The next line is the chosen base streams.
  InfoTextFStream << "Base streams. ---------\n";
  for (const auto &BaseStream : this->BaseStreams) {
    InfoTextFStream << BaseStream->getStreamId() << ' '
                    << BaseStream->formatName() << '\n';
  }
  InfoTextFStream << "Base step streams. ---------\n";
  for (const auto &BaseStepStream : this->BaseStepStreams) {
    InfoTextFStream << BaseStepStream->getStreamId() << ' '
                    << BaseStepStream->formatName() << '\n';
  }
  InfoTextFStream << "Base step root streams. ---------\n";
  for (const auto &BaseStepRootStream : this->BaseStepRootStreams) {
    InfoTextFStream << BaseStepRootStream->getStreamId() << ' '
                    << BaseStepRootStream->formatName() << '\n';
  }
  InfoTextFStream << "Chosen base streams. ---------\n";
  for (const auto &ChosenBaseStream : this->ChosenBaseStreams) {
    InfoTextFStream << ChosenBaseStream->getStreamId() << ' '
                    << ChosenBaseStream->formatName() << '\n';
  }
  InfoTextFStream << "------------------------------\n";
  // The next line is the chosen step streams.
  InfoTextFStream << "Chosen base step streams. ---------\n";
  for (const auto &ChosenBaseStepStream : this->ChosenBaseStepStreams) {
    InfoTextFStream << ChosenBaseStepStream->getStreamId() << ' '
                    << ChosenBaseStepStream->formatName() << '\n';
  }
  InfoTextFStream << "------------------------------\n";
  // The next line is all chosen base streams.
  InfoTextFStream << "All chosen base streams. ----\n";
  for (const auto &AllChosenBaseStream : this->AllChosenBaseStreams) {
    InfoTextFStream << AllChosenBaseStream->getStreamId() << ' '
                    << AllChosenBaseStream->formatName() << '\n';
  }
  InfoTextFStream << "------------------------------\n";
  /**
   * Formatting any additional text information for the subclass.
   */
  this->formatAdditionalInfoText(InfoTextFStream);
  InfoTextFStream.close();

  // Also serialize with protobuf.
  Gem5ProtobufSerializer InfoSerializer(this->InfoFullPath);
  LLVM::TDG::StreamInfo ProtobufInfo;
  ProtobufInfo.set_name(this->formatName());
  ProtobufInfo.set_id(this->getStreamId());
  ProtobufInfo.set_type(this->Inst->getOpcodeName());
  ProtobufInfo.set_loop_level(this->InnerMostLoop->getLoopDepth());
  ProtobufInfo.set_config_loop_level(this->Loop->getLoopDepth());
  ProtobufInfo.set_element_size(this->getElementSize(DataLayout));
  ProtobufInfo.set_pattern_path(this->PatternFullPath);
  ProtobufInfo.set_history_path(this->HistoryFullPath);
  for (const auto &ChosenBaseStream : this->ChosenBaseStreams) {
    ProtobufInfo.add_chosen_base_ids(ChosenBaseStream->getStreamId());
  }
  for (const auto &ChosenBaseStepStream : this->ChosenBaseStepStreams) {
    ProtobufInfo.add_chosen_base_step_ids(ChosenBaseStepStream->getStreamId());
  }
  InfoSerializer.serialize(ProtobufInfo);
}

int Stream::getElementSize(llvm::DataLayout *DataLayout) const {
  if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(this->Inst)) {
    llvm::Type *StoredType =
        StoreInst->getPointerOperandType()->getPointerElementType();
    return DataLayout->getTypeStoreSize(StoredType);
  }

  if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(this->Inst)) {
    llvm::Type *LoadedType =
        LoadInst->getPointerOperandType()->getPointerElementType();
    return DataLayout->getTypeStoreSize(LoadedType);
  }

  if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(this->Inst)) {
    return DataLayout->getTypeStoreSize(PHINode->getType());
  }

  llvm_unreachable("Unsupport stream type to get element size.");
}