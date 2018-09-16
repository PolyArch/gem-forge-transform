#include "stream/Stream.h"

Stream::Stream(TypeT _Type, const std::string &_Folder,
               const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
               size_t _LoopLevel)
    : Type(_Type), Folder(_Folder), Inst(_Inst), Loop(_Loop),
      LoopLevel(_LoopLevel), Qualified(false), Chosen(false), TotalIters(0),
      TotalAccesses(0), TotalStreams(0), Iters(1), LastAccessIters(0),
      StartId(DynamicInstruction::InvalidId), Pattern() {
  this->FullPath = this->Folder + "/" + this->formatName() + ".txt";
  this->StoreFStream.open(this->FullPath);
  assert(this->StoreFStream.is_open() && "Failed to open output stream file.");
}

Stream::~Stream() { this->StoreFStream.close(); }

void Stream::endStream() {
  const auto ComputedPattern = this->Pattern.endStream();
  this->Iters = 1;
  this->LastAccessIters = 0;
  this->TotalStreams++;
  this->StartId = DynamicInstruction::InvalidId;

  // Serialize just with text.
  this->StoreFStream << "ValPat "
                     << StreamPattern::formatValuePattern(
                            ComputedPattern.ValPattern)
                     << std::endl;
  this->StoreFStream << "AccPat "
                     << StreamPattern::formatAccessPattern(
                            ComputedPattern.AccPattern)
                     << std::endl;
#define SerializeFieldText(field)                                              \
  this->StoreFStream << #field << ' ' << ComputedPattern.field << std::endl
  SerializeFieldText(Iters);
  SerializeFieldText(Accesses);
  SerializeFieldText(Updates);
  SerializeFieldText(Base);
  SerializeFieldText(StrideI);
  SerializeFieldText(NI);
  SerializeFieldText(StrideJ);
#undef SerializeFieldText
}