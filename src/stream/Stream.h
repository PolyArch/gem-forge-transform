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

/**
 * The base class to represent all streams (IV and MEM).
 *
 * The key information stored the dependence graph, which has two version:
 * basic and chosen.
 *
 * The basic dependence graph stores all static dependence information.
 * Edges are inserted between streams configured within the same loop level.
 *
 * BaseStream: Input stream used by myself;
 * DependentStream: Stream dependent on myself;
 *
 * MEM -> IV dependency is a back-edge and stored in BackMemBaseStream
 * and BackIVDependentStream.
 *
 * The Chosen dependence graph represents the final chosen graph.
 */

class Stream {
 public:
  enum TypeT {
    IV,
    MEM,
  };
  const TypeT Type;
  Stream(TypeT _Type, const std::string &_Folder,
         const std::string &_RelativeFolder, const llvm::Instruction *_Inst,
         const llvm::Loop *_Loop, const llvm::Loop *_InnerMostLoop,
         size_t _LoopLevel, llvm::DataLayout *DataLayout);
  virtual ~Stream() = default;

  using StreamSet = std::unordered_set<Stream *>;

  /**
   * Candidate: statically determined.
   * QualifySeed: dynamically determined.
   */
  virtual bool isCandidate() const = 0;
  virtual bool isQualifySeed() const = 0;

  void markQualified() {
    assert(!this->HasMissingBaseStream &&
           "Marking a stream with missing base stream qualified.");
    this->Qualified = true;
  }
  void markUnqualified() { this->Qualified = false; }
  bool isQualified() const { return this->Qualified; }
  void markChosen() {
    assert(!this->HasMissingBaseStream &&
           "Marking a stream with missing base stream chosen.");
    this->Chosen = true;
  }
  bool isChosen() const { return this->Chosen; }
  void setCoalesceGroup(int CoalesceGroup) {
    assert(this->CoalesceGroup == -1 && "Coalesce group is already set.");
    this->CoalesceGroup = CoalesceGroup;
  }
  int getCoalesceGroup() const { return this->CoalesceGroup; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  const llvm::Loop *getInnerMostLoop() const { return this->InnerMostLoop; }
  const llvm::Instruction *getInst() const { return this->Inst; }
  uint64_t getStreamId() const { return reinterpret_cast<uint64_t>(this); }
  std::string getPatternFullPath() const {
    return this->Folder + "/" + this->PatternFileName;
  }
  std::string getInfoFullPath() const {
    return this->Folder + "/" + this->InfoFileName;
  }
  std::string getHistoryFullPath() const {
    return this->Folder + "/" + this->HistoryFileName;
  }
  std::string getPatternRelativePath() const {
    return this->RelativeFolder + "/" + this->PatternFileName;
  }
  std::string getInfoRelativePath() const {
    return this->RelativeFolder + "/" + this->InfoFileName;
  }
  std::string getHistoryRelativePath() const {
    return this->RelativeFolder + "/" + this->HistoryFileName;
  }
  std::string getTextPath(const std::string &Raw) const { return Raw + ".txt"; }

  const std::list<LLVM::TDG::StreamPattern> &getProtobufPatterns() const {
    return this->ProtobufPatterns;
  }

  size_t getLoopLevel() const { return this->LoopLevel; }
  size_t getTotalIters() const { return this->TotalIters; }
  size_t getTotalAccesses() const { return this->TotalAccesses; }
  size_t getTotalStreams() const { return this->TotalStreams; }
  const StreamPattern &getPattern() const { return this->Pattern; }

  /**
   * This must happen after all the calls to addBaseStream.
   */
  void computeBaseStepRootStreams();

  void endIter() {
    if (this->LastAccessIters != this->Iters) {
      this->Pattern.addMissingAccess();
      this->LastAccessIters = this->Iters;
    }
    this->Iters++;
    this->TotalIters++;
  }

  virtual void addAccess(uint64_t Addr,
                         DynamicInstruction::DynamicId DynamicId) {
    assert(false && "addAccess in Stream is not implemented.");
  }
  virtual void addAccess(const DynamicValue &DynamicVal) {
    assert(false && "addAccess for IV in Stream is not implemented.");
  }

  void endStream();

  void finalizePattern();
  void finalizeInfo(llvm::DataLayout *DataLayout);

  void fillProtobufStreamInfo(llvm::DataLayout *DataLayout,
                              LLVM::TDG::StreamInfo *ProtobufInfo) const;

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
    // We need a more compact encoding of a stream name. Since the function is
    // always the same, let it be (type function loop_header_bb inst_bb
    // inst_name)
    return "(" + this->formatType() + " " +
           Utils::formatLLVMFunc(this->Inst->getFunction()) + " " +
           this->Loop->getHeader()->getName().str() + " " +
           Utils::formatLLVMInstWithoutFunc(this->Inst) + ")";
  }

  virtual const std::unordered_set<const llvm::Instruction *> &getComputeInsts()
      const = 0;

  virtual const std::unordered_set<const llvm::Instruction *> &getStepInsts()
      const {
    assert(false && "getStepInst only implemented for IVStream.");
  }

  virtual const std::unordered_set<const llvm::LoadInst *> &getBaseLoads()
      const = 0;

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

  /**
   * Accessors for stream sets.
   */
  bool hasNoBaseStream() const { return this->BaseStreams.empty(); }
  const StreamSet &getBaseStreams() const { return this->BaseStreams; }
  const StreamSet &getBackMemBaseStreams() const {
    return this->BackMemBaseStreams;
  }
  const StreamSet &getBackIVDependentStreams() const {
    return this->BackIVDependentStreams;
  }

  const StreamSet &getBaseStepRootStreams() const {
    return this->BaseStepRootStreams;
  }
  const StreamSet &getChosenBaseStreams() const {
    return this->ChosenBaseStreams;
  }
  const StreamSet &getChosenBaseStepStreams() const {
    return this->ChosenBaseStepStreams;
  }
  const StreamSet &getAllChosenBaseStreams() const {
    return this->AllChosenBaseStreams;
  }
  const StreamSet &getDependentStreams() const {
    return this->DependentStreams;
  }
  const StreamSet &getAllChosenDependentStreams() const {
    return this->AllChosenDependentStreams;
  }

  /**
   * Build the dependence graph by add all the base streams.
   */
  using GetStreamFuncT =
      std::function<Stream *(const llvm::Instruction *, const llvm::Loop *)>;
  virtual void buildBasicDependenceGraph(GetStreamFuncT GetStream) = 0;

  using GetChosenStreamFuncT =
      std::function<Stream *(const llvm::Instruction *)>;
  virtual void buildChosenDependenceGraph(
      GetChosenStreamFuncT GetChosenStream) = 0;

 protected:
  /**
   * Stores the information of the stream.
   */
  std::string Folder;
  std::string RelativeFolder;
  std::string PatternFileName;
  std::string InfoFileName;
  std::string HistoryFileName;
  std::list<LLVM::TDG::StreamPattern> ProtobufPatterns;

  /**
   * Stores the instruction and loop.
   */
  const llvm::Instruction *Inst;
  const llvm::Loop *Loop;
  const llvm::Loop *InnerMostLoop;
  const size_t LoopLevel;

  size_t ElementSize;

  StreamSet BaseStreams;
  StreamSet DependentStreams;
  StreamSet BackMemBaseStreams;
  StreamSet BackIVDependentStreams;

  StreamSet BaseStepStreams;
  StreamSet BaseStepRootStreams;

  StreamSet ChosenBaseStreams;
  StreamSet ChosenDependentStreams;
  StreamSet ChosenBackMemBaseStreams;
  StreamSet ChosenBackIVDependentStreams;

  StreamSet ChosenBaseStepStreams;
  StreamSet ChosenBaseStepRootStreams;

  StreamSet AllChosenBaseStreams;
  StreamSet AllChosenDependentStreams;
  bool HasMissingBaseStream;
  bool Qualified;
  bool Chosen;
  bool IsStaticStream;

  int CoalesceGroup;
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

  void addBaseStream(Stream *Other);
  void addBackEdgeBaseStream(Stream *Other);

  void addChosenBaseStream(Stream *Other);
  void addChosenBackEdgeBaseStream(Stream *Other);
  void addAllChosenBaseStream(Stream *Other);
};

#endif
