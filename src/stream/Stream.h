#ifndef LLVM_TDG_STREAM_STREAM_H
#define LLVM_TDG_STREAM_STREAM_H

#include "stream/StreamMessage.pb.h"

#include "Gem5ProtobufSerializer.h"
#include "LoopUtils.h"
#include "Utils.h"
#include "stream/StreamPattern.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"

#include <fstream>
#include <string>
#include <unordered_set>

class Stream {
public:
  enum TypeT {
    IV,
    MEM,
  };
  const TypeT Type;
  Stream(TypeT _Type, const std::string &_Folder,
         const llvm::Instruction *_Inst, const llvm::Loop *_Loop,
         const llvm::Loop *_InnerMostLoop, size_t _LoopLevel);

  bool hasNoBaseStream() const { return this->BaseStreams.empty(); }
  const std::unordered_set<Stream *> &getBaseStreams() const {
    return this->BaseStreams;
  }
  const std::unordered_set<Stream *> &getChosenBaseStreams() const {
    return this->ChosenBaseStreams;
  }
  const std::unordered_set<Stream *> &getChosenBaseStepStreams() const {
    return this->ChosenBaseStepStreams;
  }
  const std::unordered_set<Stream *> &getAllChosenBaseStreams() const {
    return this->AllChosenBaseStreams;
  }
  const std::unordered_set<Stream *> &getDependentStreams() const {
    return this->DependentStreams;
  }
  /**
   * Interface to decide even if the stream can be considered as a candidate
   * (before mark it qualified or not).
   */
  virtual bool isCandidate() const { return true; }
  void markQualified() {
    assert(!this->HasMissingBaseStream &&
           "Marking a stream with missing base stream qualified.");
    this->Qualified = true;
  }
  bool isQualified() const { return this->Qualified; }
  void markChosen() {
    assert(!this->HasMissingBaseStream &&
           "Marking a stream with missing base stream chosen.");
    this->Chosen = true;
  }
  bool isChosen() const { return this->Chosen; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const llvm::Loop *getInnerMostLoop() const { return this->InnerMostLoop; }
  const llvm::Instruction *getInst() const { return this->Inst; }
  uint64_t getStreamId() const { return reinterpret_cast<uint64_t>(this); }
  const std::string &getPatternPath() const { return this->PatternFullPath; }
  const std::string &getInfoPath() const { return this->InfoFullPath; }
  const std::string &getHistoryPath() const { return this->HistoryFullPath; }
  const std::string &getHistoryTextPath() const {
    return this->HistoryTextFullPath;
  }
  size_t getLoopLevel() const { return this->LoopLevel; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  const StreamPattern &getPattern() const { return this->Pattern; }

  /**
   * A hacky idea is to use nullptr to represent an missing stream.
   * This base stream is never qualified. This is to solve the case when the
   * needing base stream is not instantiated at a specific level.
   */
  void addBaseStream(Stream *Other);
  void addChosenBaseStream(Stream *Other);
  void addAllChosenBaseStream(Stream *Other);

  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addMissingAccess();
      this->LastAccessIters = this->Iters;
    }
    this->Iters++;
    this->TotalIters++;
  }

  void endStream();

  void finalize(llvm::DataLayout *DataLayout);

  virtual bool isAliased() const { return false; }
  std::string formatType() const {
    switch (this->Type) {
    case IV:
      return "IV";
    case MEM:
      return "MEM";
    default: { llvm_unreachable("Illegal stream type to be formatted."); }
    }
  }
  std::string formatName() const {
    return "(" + this->formatType() + " " + LoopUtils::getLoopId(this->Loop) +
           " " + LoopUtils::formatLLVMInst(this->Inst) + ")";
  }
  virtual const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const = 0;

  static bool isStepInst(const llvm::Instruction *Inst) {
    auto Opcode = Inst->getOpcode();
    switch (Opcode) {
    case llvm::Instruction::Add: {
      return true;
    }
    case llvm::Instruction::GetElementPtr: {
      return true;
    }
    default: { return false; }
    }
  }

protected:
  /**
   * Stores the information of the stream.
   */
  std::string Folder;
  std::string PatternFullPath;
  std::string InfoFullPath;
  std::string HistoryFullPath;
  std::string PatternTextFullPath;
  std::string InfoTextFullPath;
  std::string HistoryTextFullPath;
  LLVM::TDG::StreamPattern ProtobufPattern;
  Gem5ProtobufSerializer *PatternSerializer;
  std::ofstream PatternTextFStream;

  /**
   * Stores the instruction and loop.
   */
  const llvm::Instruction *Inst;
  const llvm::Loop *Loop;
  const llvm::Loop *InnerMostLoop;
  const size_t LoopLevel;

  std::unordered_set<Stream *> BaseStreams;
  std::unordered_set<Stream *> DependentStreams;
  std::unordered_set<Stream *> ChosenBaseStreams;
  std::unordered_set<Stream *> ChosenBaseStepStreams;
  std::unordered_set<Stream *> AllChosenBaseStreams;
  bool HasMissingBaseStream;
  bool Qualified;
  bool Chosen;
  /**
   * Stores the total iterations for this stream
   */
  size_t TotalIters;
  size_t TotalAccesses;
  size_t TotalStreams;
  /**
   * Maintain the current iteration. Will be reset by endStream() and update by
   * endIter().
   * It should start at 1.
   */
  size_t Iters;
  /**
   * Maintain the iteration when the last addAccess() is called.
   * When endIter(), we check that LastAccessIters == Iters to detect missint
   * access in the last iteration.
   * It should be reset to 0 (should be less than reset value of Iters).
   */
  size_t LastAccessIters;

  /**
   * Stores the dynamic id of the first access in the current stream.
   * Bad design: Only used by MemStream. IVStream will not dynamic id as it is
   * based on phi inst which will be removed from the data graph.
   */
  DynamicInstruction::DynamicId StartId;

  StreamPattern Pattern;

  int getElementSize(llvm::DataLayout *DataLayout) const;

  /**
   * Used for subclass to add additionl dumping information.
   */
  virtual void formatAdditionalInfoText(std::ostream &OStream) const {}
};

#endif