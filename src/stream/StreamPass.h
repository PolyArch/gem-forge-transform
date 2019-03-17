#ifndef LLVM_TDG_STREAM_PASS_H
#define LLVM_TDG_STREAM_PASS_H

#include "Replay.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"
#include "stream/MemStream.h"
#include "stream/StreamRegionAnalyzer.h"

extern llvm::cl::opt<StreamPassChooseStrategyE> StreamPassChooseStrategy;

class StreamStepInst : public DynamicInstruction {
 public:
  StreamStepInst(Stream *_S, DynamicId _Id = InvalidId)
      : DynamicInstruction(), S(_S) {
    /**
     * Inherit the provided dynamic id if provided a valid id.
     */
    if (_Id != InvalidId) {
      this->Id = _Id;
    }
  }
  std::string getOpName() const override { return "stream-step"; }

 protected:
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    auto StreamStepExtra = ProtobufEntry->mutable_stream_step();
    assert(ProtobufEntry->has_stream_step() &&
           "The protobuf entry should have stream step extra struct.");
    StreamStepExtra->set_stream_id(this->S->getStreamId());
  }

 private:
  Stream *S;
};

class StreamStoreInst : public DynamicInstruction {
 public:
  StreamStoreInst(Stream *_S, DynamicId _Id = InvalidId)
      : DynamicInstruction(), S(_S) {
    /**
     * Inherit the provided dynamic id if provided a valid id.
     */
    if (_Id != InvalidId) {
      this->Id = _Id;
    }
  }
  std::string getOpName() const override { return "stream-store"; }

 protected:
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    auto StreamStoreExtra = ProtobufEntry->mutable_stream_store();
    assert(ProtobufEntry->has_stream_store() &&
           "The protobuf entry should have stream store extra struct.");
    StreamStoreExtra->set_stream_id(this->S->getStreamId());
  }

 private:
  Stream *S;
};

class StreamConfigInst : public DynamicInstruction {
 public:
  StreamConfigInst(Stream *_S) : DynamicInstruction(), S(_S) {}
  std::string getOpName() const override { return "stream-config"; }

 protected:
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    auto ConfigExtra = ProtobufEntry->mutable_stream_config();
    assert(ProtobufEntry->has_stream_config() &&
           "The protobuf entry should have stream config extra struct.");
    ConfigExtra->set_stream_name(this->S->formatName());
    ConfigExtra->set_stream_id(this->S->getStreamId());
    ConfigExtra->set_info_path(this->S->getInfoRelativePath());
  }

 private:
  Stream *S;
};

class StreamEndInst : public DynamicInstruction {
 public:
  StreamEndInst(Stream *_S) : DynamicInstruction(), S(_S) {}
  std::string getOpName() const override { return "stream-end"; }

 protected:
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    auto EndExtra = ProtobufEntry->mutable_stream_end();
    assert(ProtobufEntry->has_stream_end() &&
           "The protobuf entry should have stream end extra struct.");
    EndExtra->set_stream_id(this->S->getStreamId());
  }

 private:
  Stream *S;
};

class StreamPass : public ReplayTrace {
 public:
  static char ID;
  StreamPass(char _ID = ID)
      : ReplayTrace(_ID),
        DynInstCount(0),
        DynMemInstCount(0),
        StepInstCount(0),
        ConfigInstCount(0),
        DeletedInstCount(0) {}

  StreamRegionAnalyzer *getAnalyzerByLoop(const llvm::Loop *Loop) const;

 protected:
  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;
  void dumpStats(std::ostream &O) override;
  void transform() override;

  using ActiveIVStreamMapT = std::unordered_map<
      const llvm::Loop *,
      std::unordered_map<const llvm::PHINode *, InductionVarStream *>>;
  using LoopStackT = std::list<llvm::Loop *>;

  std::unordered_map<const llvm::Loop *, std::unique_ptr<StreamRegionAnalyzer>>
      LoopStreamAnalyzerMap;
  StreamRegionAnalyzer *CurrentStreamAnalyzer;
  uint64_t RegionIdx;

  /*************************************************************
   * Stream Analysis.
   *************************************************************/

  void analyzeStream();
  bool isLoopContinuous(const llvm::Loop *Loop);
  void addAccess(DynamicInstruction *DynamicInst);

  void pushLoopStack(LoopStackT &LoopStack, llvm::Loop *NewLoop);
  void popLoopStack(LoopStackT &LoopStack);

  /*************************************************************
   * Stream transform.
   *************************************************************/
  /**
   * Maps from a stream to its last config/step instruction.
   */
  using ActiveStreamInstMapT =
      std::unordered_map<const llvm::Instruction *,
                         DynamicInstruction::DynamicId>;
  void pushLoopStackAndConfigureStreams(
      LoopStackT &LoopStack, llvm::Loop *NewLoop,
      DataGraph::DynamicInstIter NewInstIter,
      ActiveStreamInstMapT &ActiveStreamInstMap);
  void popLoopStackAndUnconfigureStreams(
      LoopStackT &LoopStack, DataGraph::DynamicInstIter NewInstIter,
      ActiveStreamInstMapT &ActiveStreamInstMap);
  void DEBUG_TRANSFORMED_STREAM(DynamicInstruction *DynamicInst);
  virtual void transformStream();

  /************************************************************
   * Memorization.
   ************************************************************/
  std::unordered_set<const llvm::Loop *> InitializedLoops;
  std::unordered_map<const llvm::Loop *,
                     std::unordered_set<llvm::Instruction *>>
      MemorizedStreamInst;
  std::unordered_map<const llvm::Loop *, int> MemorizedLoopPossiblePaths;
  std::unordered_map<const llvm::Loop *, bool> MemorizedLoopContinuous;

  std::unordered_map<llvm::Instruction *, uint64_t> MemAccessInstCount;

  /*****************************************
   * Statistics.
   */
  uint64_t DynInstCount;
  uint64_t DynMemInstCount;
  uint64_t StepInstCount;
  uint64_t ConfigInstCount;
  uint64_t DeletedInstCount;
};

#endif