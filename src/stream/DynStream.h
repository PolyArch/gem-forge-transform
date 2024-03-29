#ifndef LLVM_TDG_STREAM_STREAM_H
#define LLVM_TDG_STREAM_STREAM_H

#include "Gem5ProtobufSerializer.h"
#include "LoopUtils.h"
#include "Utils.h"
#include "stream/StaticStream.h"
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

class DynStream {
public:
  using IsIndVarFunc = std::function<bool(const llvm::PHINode *)>;
  StaticStream *SStream;
  DynStream(const std::string &_Folder, const std::string &_RelativeFolder,
            StaticStream *_SStream, llvm::DataLayout *DataLayout);
  virtual ~DynStream() = default;

  using StreamSet = std::unordered_set<DynStream *>;

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
  void markChosen() { this->SStream->markChosen(); }
  bool isChosen() const { return this->SStream->isChosen(); }
  void setRegionStreamId(int Id) { this->SStream->setRegionStreamId(Id); }
  int getRegionStreamId() const { return this->SStream->getRegionStreamId(); }

  void setCoalesceGroup(uint64_t CoalesceGroup, int32_t CoalesceOffset = -1) {
    this->SStream->setCoalesceGroup(CoalesceGroup, CoalesceOffset);
  }
  int getCoalesceGroup() const { return this->SStream->getCoalesceGroup(); }

  const llvm::Loop *getLoop() const { return this->SStream->ConfigureLoop; }
  const llvm::Loop *getInnerMostLoop() const {
    return this->SStream->InnerMostLoop;
  }
  const llvm::Instruction *getInst() const { return this->SStream->Inst; }
  uint64_t getStreamId() const { return SStream->StreamId; }
  std::string getPatternFullPath() const {
    return this->Folder + "/" + this->PatternFileName;
  }
  std::string getHistoryFullPath() const {
    return this->Folder + "/" + this->HistoryFileName;
  }
  std::string getPatternRelativePath() const {
    return this->RelativeFolder + "/" + this->PatternFileName;
  }
  std::string getHistoryRelativePath() const {
    return this->RelativeFolder + "/" + this->HistoryFileName;
  }
  std::string getTextPath(const std::string &Raw) const { return Raw + ".txt"; }

  const std::list<LLVM::TDG::StreamPattern> &getProtobufPatterns() const {
    return this->ProtobufPatterns;
  }

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

  void fillProtobufStreamInfo(llvm::DataLayout *DataLayout,
                              LLVM::TDG::StreamInfo *ProtobufInfo) const;

  virtual bool isAliased() const { return false; }
  std::string getStreamName() const { return this->SStream->getStreamName(); }

  virtual const std::unordered_set<const llvm::Instruction *> &
  getComputeInsts() const = 0;

  virtual const std::unordered_set<const llvm::Instruction *> &
  getStepInsts() const {
    assert(false && "getStepInst only implemented for IVStream.");
  }

  virtual const std::unordered_set<const llvm::LoadInst *> &
  getBaseLoads() const = 0;

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
  const DynStream *getSingleStepRootStream() const {
    assert(this->BaseStepRootStreams.size() <= 1 &&
           "Multiple StepRootStreams.");
    if (this->BaseStepRootStreams.empty()) {
      return nullptr;
    } else {
      return *(this->BaseStepRootStreams.begin());
    }
  }
  const StreamSet &getChosenBaseStreams() const {
    return this->ChosenBaseStreams;
  }
  const StreamSet &getChosenDependentStreams() const {
    return this->ChosenDependentStreams;
  }
  const StreamSet &getChosenBackIVDependentStreams() const {
    return this->ChosenBackIVDependentStreams;
  }
  const StreamSet &getChosenBaseStepStreams() const {
    return this->ChosenBaseStepStreams;
  }
  const DynStream *getSingleChosenStepRootStream() const {
    assert(this->ChosenBaseStepRootStreams.size() <= 1 &&
           "Multiple chosen StepRootStreams.");
    if (this->ChosenBaseStepRootStreams.empty()) {
      return nullptr;
    } else {
      return *(this->ChosenBaseStepRootStreams.begin());
    }
  }
  const StreamSet &getDependentStreams() const {
    return this->DependentStreams;
  }

  /**
   * Build the dependence graph by add all the base streams.
   */
  using GetStreamFuncT =
      std::function<DynStream *(const llvm::Instruction *, const llvm::Loop *)>;
  virtual void buildBasicDependenceGraph(GetStreamFuncT GetStream) = 0;

  using GetChosenStreamFuncT =
      std::function<DynStream *(const llvm::Instruction *)>;
  void buildChosenDependenceGraph(GetChosenStreamFuncT GetChosenStream);

  /**
   * Get the input value for addr/reduce/pred func when configure it.
   */
  using InputValueList = StaticStream::InputValueList;

  InputValueList getExecFuncInputValues(const ExecutionDataGraph &ExecDG) const;
  const DynStream *getExecFuncInputStream(const llvm::Value *Value) const;

  int getMemElementSize() const { return this->SStream->getMemElementSize(); }
  int getCoreElementSize() const { return this->SStream->getCoreElementSize(); }

protected:
  /**
   * Stores the information of the stream.
   */
  std::string Folder;
  std::string RelativeFolder;
  std::string PatternFileName;
  std::string HistoryFileName;
  std::list<LLVM::TDG::StreamPattern> ProtobufPatterns;

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

  bool HasMissingBaseStream;
  bool Qualified;

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

  /**
   * Used for subclass to add additionl dumping information.
   */
  virtual void formatAdditionalInfoText(std::ostream &OStream) const {}

  void addBaseStream(DynStream *Other);
  void addBackEdgeBaseStream(DynStream *Other);
};

#endif
