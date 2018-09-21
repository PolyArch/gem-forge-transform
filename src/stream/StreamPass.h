#ifndef LLVM_TDG_STREAM_PASS_H
#define LLVM_TDG_STREAM_PASS_H

#include "Replay.h"
#include "Utils.h"
#include "stream/InductionVarStream.h"
#include "stream/MemStream.h"

struct StreamTransformPlan {
public:
  enum PlanT {
    NOTHING,
    DELETE,
    STEP,
    STORE,
  } Plan;

  static std::string formatPlanT(const PlanT Plan) {
    switch (Plan) {
    case NOTHING:
      return "NOTHING";
    case DELETE:
      return "DELETE";
    case STEP:
      return "STEP";
    case STORE:
      return "STORE";
    }
    llvm_unreachable("Illegal StreamTransformPlan::PlanT to be formatted.");
  }

  StreamTransformPlan() : Plan(NOTHING) {}

  const std::unordered_set<Stream *> &getUsedStreams() const {
    return this->UsedStreams;
  }
  Stream *getParamStream() const { return this->ParamStream; }

  void addUsedStream(Stream *UsedStream) {
    this->UsedStreams.insert(UsedStream);
  }

  void planToDelete() { this->Plan = DELETE; }
  void planToStep(Stream *S) {
    this->ParamStream = S;
    this->Plan = STEP;
  }
  void planToStore(Stream *S) {
    this->ParamStream = S;
    this->Plan = STORE;
    // Store is also considered as use.
    this->addUsedStream(S);
  }

  std::string format() const;

private:
  std::unordered_set<Stream *> UsedStreams;
  Stream *ParamStream;
};

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
    ConfigExtra->set_pattern_path(this->S->getPatternPath());
    ConfigExtra->set_info_path(this->S->getInfoPath());
  }

private:
  Stream *S;
};

class StreamPass : public ReplayTrace {
public:
  static char ID;
  StreamPass(char _ID = ID)
      : ReplayTrace(_ID), DynInstCount(0), DynMemInstCount(0), StepInstCount(0),
        ConfigInstCount(0), DeletedInstCount(0) {}

protected:
  bool initialize(llvm::Module &Module) override;
  bool finalize(llvm::Module &Module) override;
  void dumpStats(std::ostream &O) override;
  void transform() override;

  std::string classifyStream(const MemStream &S) const;
  using ActiveStreamMapT =
      std::unordered_map<const llvm::Loop *,
                         std::unordered_map<llvm::Instruction *, MemStream *>>;
  using ActiveIVStreamMapT = std::unordered_map<
      const llvm::Loop *,
      std::unordered_map<const llvm::PHINode *, InductionVarStream *>>;
  using LoopStackT = std::list<llvm::Loop *>;

  /*************************************************************
   * Stream Analysis.
   *************************************************************/

  void analyzeStream();
  bool isLoopContinuous(const llvm::Loop *Loop);
  void addAccess(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                 DynamicInstruction *DynamicInst);

  void handleAlias(DynamicInstruction *DynamicInst);

  void endIter(const LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
               ActiveIVStreamMapT &ActiveIVStreams);
  void pushLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                     ActiveIVStreamMapT &ActiveIVStreams, llvm::Loop *NewLoop);
  void popLoopStack(LoopStackT &LoopStack, ActiveStreamMapT &ActiveStreams,
                    ActiveIVStreamMapT &ActiveIVStreams);
  void activateStream(ActiveStreamMapT &ActiveStreams,
                      llvm::Instruction *Instruction);
  void activateIVStream(ActiveIVStreamMapT &ActiveIVStreams,
                        llvm::PHINode *PHINode);
  void initializeMemStreamIfNecessary(const LoopStackT &LoopStack,
                                      llvm::Instruction *Inst);
  void initializeIVStreamIfNecessary(const LoopStackT &LoopStack,
                                     llvm::PHINode *Inst);

  /*************************************************************
   * Stream Choice.
   *************************************************************/
  void chooseStream();
  void buildStreamDependenceGraph();
  void markQualifiedStream();
  void addChosenStream(const llvm::Loop *Loop, const llvm::Instruction *Inst,
                       Stream *S);
  void buildChosenStreamDependenceGraph();
  void buildAllChosenStreamDependenceGraph();
  MemStream *getMemStreamByInstLoop(llvm::Instruction *Inst,
                                    const llvm::Loop *Loop);

  /*************************************************************
   * Stream transform.
   *************************************************************/
  /**
   * Maps from a stream to its last config/step instruction.
   */
  using ActiveStreamInstMapT =
      std::unordered_map<const llvm::Instruction *,
                         DynamicInstruction::DynamicId>;
  void makeStreamTransformPlan();
  StreamTransformPlan &getOrCreatePlan(const llvm::Instruction *Inst);
  StreamTransformPlan *getPlanNullable(const llvm::Instruction *Inst);
  void DEBUG_PLAN_FOR_LOOP(const llvm::Loop *Loop);
  void DEBUG_SORTED_STREAMS_FOR_LOOP(const llvm::Loop *Loop);
  void sortChosenStream(Stream *S, std::list<Stream *> &Stack,
                        std::unordered_set<Stream *> &SortedStreams);
  void
  pushLoopStackAndConfigureStreams(LoopStackT &LoopStack, llvm::Loop *NewLoop,
                                   DataGraph::DynamicInstIter NewInstIter,
                                   ActiveStreamInstMapT &ActiveStreamInstMap);
  void
  popLoopStackAndUnconfigureStreams(LoopStackT &LoopStack,
                                    ActiveStreamInstMapT &ActiveStreamInstMap);
  void DEBUG_TRANSFORMED_STREAM(DynamicInstruction *DynamicInst);
  virtual void transformStream();

  std::unordered_map<const llvm::Instruction *, std::list<Stream *>>
      InstStreamMap;

  std::unordered_map<llvm::Instruction *, std::list<MemStream>>
      InstMemStreamMap;

  std::unordered_map<const llvm::PHINode *, std::list<InductionVarStream>>
      PHINodeIVStreamMap;

  std::unordered_map<const llvm::Loop *,
                     std::unordered_map<const llvm::Instruction *, Stream *>>
      ChosenLoopInstStream;
  // Sorted the streams in dependence order.
  // This is the order we will configure the stream.
  std::unordered_map<const llvm::Loop *, std::list<Stream *>>
      ChosenLoopSortedStreams;

  std::unordered_map<const llvm::Instruction *, StreamTransformPlan>
      InstPlanMap;

  /************************************************************
   * Memorization.
   ************************************************************/
  std::unordered_set<const llvm::Loop *> InitializedLoops;
  std::unordered_map<const llvm::Loop *,
                     std::unordered_set<llvm::Instruction *>>
      MemorizedStreamInst;
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