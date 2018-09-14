#include "stream/Stream.h"

Stream::Stream(TypeT _Type, const std::string &_Folder,
               const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
               size_t _LoopLevel)
    : Type(_Type), Folder(_Folder), Inst(_Inst), Loop(_Loop),
      LoopLevel(_LoopLevel), Qualified(false), TotalIters(0), TotalAccesses(0),
      TotalStreams(0), Iters(1), LastAccessIters(0), Pattern() {
  this->FullPath = this->Folder + "/" + this->formatName() + ".txt";
  this->StoreFStream.open(this->FullPath);
  assert(this->StoreFStream.is_open() && "Failed to open output stream file.");
}

Stream::~Stream() { this->StoreFStream.close(); }