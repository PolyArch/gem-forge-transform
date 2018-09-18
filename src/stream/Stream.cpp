#include "stream/Stream.h"

Stream::Stream(TypeT _Type, const std::string &_Folder,
               const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
               size_t _LoopLevel)
    : Type(_Type), Folder(_Folder), Inst(_Inst), Loop(_Loop),
      LoopLevel(_LoopLevel), Qualified(false), Chosen(false), TotalIters(0),
      TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
      StartId(DynamicInstruction::InvalidId), Pattern() {
  this->PatternFullPath = this->Folder + "/" + this->formatName() + ".pattern";
  this->PatternTextFullPath = this->PatternFullPath + ".txt";
  this->InfoFullPath = this->Folder + "/" + this->formatName() + ".info";
  this->InfoTextFullPath = this->InfoFullPath + ".txt";
  this->PatternSerializer = new Gem5ProtobufSerializer(this->PatternFullPath);
  this->PatternTextFStream.open(this->PatternTextFullPath);
  assert(this->PatternTextFStream.is_open() &&
         "Failed to open output stream file.");
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
  this->PatternSerializer->serialize(this->ProtobufPattern);
}

void Stream::finalize() {
  this->Pattern.finalizePattern();
  this->PatternTextFStream.close();
  delete this->PatternSerializer;
  this->PatternSerializer = nullptr;

  std::ofstream InfoTextFStream(this->InfoTextFullPath);
  assert(InfoTextFStream.is_open() && "Failed to open the output info file.");

  InfoTextFStream << this->formatName() << '\n';
  InfoTextFStream << this->getStreamId() << '\n';
  // The next line is the chosen base streams.
  InfoTextFStream << "Chosen base streams. ---------\n";
  for (const auto &ChosenBaseStream : this->ChosenBaseStreams) {
    InfoTextFStream << ChosenBaseStream->getStreamId() << ' '
                    << ChosenBaseStream->formatName() << '\n';
  }
  InfoTextFStream << "------------------------------\n";
  // The next line is all chosen base streams.
  InfoTextFStream << "Root chosen base streams. ----\n";
  for (const auto &AllChosenBaseStream : this->AllChosenBaseStreams) {
    InfoTextFStream << AllChosenBaseStream->getStreamId() << ' '
                    << AllChosenBaseStream->formatName() << '\n';
  }
  InfoTextFStream << "------------------------------\n";
  InfoTextFStream.close();

  // Also serialize with protobuf.
  Gem5ProtobufSerializer InfoSerializer(this->InfoFullPath);
  LLVM::TDG::StreamInfo ProtobufInfo;
  ProtobufInfo.set_name(this->formatName());
  ProtobufInfo.set_id(this->getStreamId());
  for (const auto &ChosenBaseStream : this->ChosenBaseStreams) {
    ProtobufInfo.add_chosen_base_ids(ChosenBaseStream->getStreamId());
  }
  InfoSerializer.serialize(ProtobufInfo);
}