#include "stream/Stream.h"
#include "stream/StreamMessage.pb.h"

Stream::Stream(TypeT _Type, const std::string &_Folder,
               const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
               size_t _LoopLevel)
    : Type(_Type), Folder(_Folder), Inst(_Inst), Loop(_Loop),
      LoopLevel(_LoopLevel), Qualified(false), Chosen(false), TotalIters(0),
      TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
      StartId(DynamicInstruction::InvalidId), Pattern() {
  this->PatternFullPath =
      this->Folder + "/" + this->formatName() + ".pattern.txt";
  this->InfoFullPath = this->Folder + "/" + this->formatName() + ".info.txt";
  this->PatternStoreFStream.open(this->PatternFullPath);
  assert(this->PatternStoreFStream.is_open() &&
         "Failed to open output stream file.");
}

void Stream::endStream() {
  const auto ComputedPattern = this->Pattern.endStream();
  this->Iters = 1;
  this->LastAccessIters = 0;
  this->TotalStreams++;
  this->StartId = DynamicInstruction::InvalidId;

  // Serialize just with text.
  this->PatternStoreFStream
      << "ValPat "
      << StreamPattern::formatValuePattern(ComputedPattern.ValPattern)
      << std::endl;
  this->PatternStoreFStream
      << "AccPat "
      << StreamPattern::formatAccessPattern(ComputedPattern.AccPattern)
      << std::endl;
#define SerializeFieldText(field)                                              \
  this->PatternStoreFStream << #field << ' ' << ComputedPattern.field          \
                            << std::endl
  SerializeFieldText(Iters);
  SerializeFieldText(Accesses);
  SerializeFieldText(Updates);
  SerializeFieldText(Base);
  SerializeFieldText(StrideI);
  SerializeFieldText(NI);
  SerializeFieldText(StrideJ);
#undef SerializeFieldText
}

void Stream::finalize() {
  this->Pattern.finalizePattern();
  this->PatternStoreFStream.close();
  std::ofstream InfoFStream(this->InfoFullPath);
  assert(InfoFStream.is_open() && "Failed to open the output info file.");

  InfoFStream << this->formatName() << '\n';
  InfoFStream << this->getStreamId() << '\n';
  // The next line is the chosen base streams.
  for (const auto &ChosenBaseStream : this->ChosenBaseStreams) {
    InfoFStream << ChosenBaseStream->getStreamId() << ' ';
  }
  InfoFStream << '\n';
  // The next line is all chosen base streams.
  for (const auto &AllChosenBaseStream : this->AllChosenBaseStreams) {
    InfoFStream << AllChosenBaseStream->getStreamId() << ' ';
  }
  InfoFStream << '\n';

  InfoFStream.close();
}