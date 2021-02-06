#ifndef LLVM_TDG_STREAM_STATIC_STREAM_H
#define LLVM_TDG_STREAM_STATIC_STREAM_H

#include "StreamPassOptions.h"
#include "StreamUtils.h"

#include "BasicBlockPredicate.h"
#include "LoopUtils.h"
#include "PostDominanceFrontier.h"
#include "stream/ae/StreamDataGraph.h"

#include "stream/StreamMessage.pb.h"

#include <list>
#include <unordered_set>

/**
 * A stream maintains two graphs:
 *
 * MetaGraph: Nodes can be ComputeMetaNode or PHIMetaNode. Leaf can be base
 * streams or inputs.
 *
 * StreamGraph:
 *   Nodes are StaticStream. Encode the dependence between streams. BackEdge
 * dependence from IndVarStream to MemStream within the same InnerMostLoop is
 * separate in BackMemBaseStreams.
 *
 */
class StaticStream {
public:
  const uint64_t StreamId;
  enum TypeT {
    IV,
    MEM,
    USER,
  };
  const TypeT Type;
  const llvm::Instruction *const Inst;
  const llvm::Loop *const ConfigureLoop;
  const llvm::Loop *const InnerMostLoop;
  const std::string FuncNameBase;
  llvm::ScalarEvolution *const SE;
  const llvm::PostDominatorTree *PDT;
  llvm::DataLayout *DataLayout;

  /**
   * The constructor just creates the object and does not perform any analysis.
   *
   * After creating all the streams, the manager should call constructGraph() to
   * initialize the MetaGraph and StreamGraph.
   */
  StaticStream(TypeT _Type, const llvm::Instruction *_Inst,
               const llvm::Loop *_ConfigureLoop,
               const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE,
               const llvm::PostDominatorTree *_PDT,
               llvm::DataLayout *_DataLayout);
  virtual ~StaticStream() {}
  void setStaticStreamInfo(LLVM::TDG::StaticStreamInfo &SSI) const;
  void fillProtobufStreamInfo(LLVM::TDG::StreamInfo *ProtobufInfo) const;
  int getCoreElementSize() const;
  int getMemElementSize() const;
  llvm::Type *getCoreElementType() const;
  llvm::Type *getMemElementType() const;

  std::string formatType() const {
    switch (this->Type) {
    case StaticStream::IV:
      return "IV";
    case StaticStream::MEM:
      return "MEM";
    default: {
      llvm_unreachable("Illegal stream type to be formatted.");
    }
    }
  }

  std::string formatName() const;
  void formatProtoStreamId(::LLVM::TDG::StreamId *ProtoId) const {
    ProtoId->set_id(this->StreamId);
    ProtoId->set_name(this->formatName());
  }

  /**
   * This represents the bidirectional dependence graph between streams.
   */
  using StreamSet = std::unordered_set<StaticStream *>;
  using StreamVec = std::vector<StaticStream *>;
  StreamSet BaseStreams;
  StreamSet DependentStreams;
  StreamSet BackMemBaseStreams;
  StreamSet BackIVDependentStreams;
  StreamSet BaseStepStreams;
  StreamSet BaseStepRootStreams;
  // Simple Load-Store dependence.
  StreamSet LoadStoreDepStreams;
  StreamSet LoadStoreBaseStreams;

  // Chosen graphs.
  StreamSet ChosenBaseStreams;
  StreamSet ChosenDependentStreams;
  StreamSet ChosenBackMemBaseStreams;
  StreamSet ChosenBackIVDependentStreams;
  StreamSet ChosenBaseStepStreams;
  StreamSet ChosenBaseStepRootStreams;

  using GetStreamFuncT = std::function<StaticStream *(const llvm::Instruction *,
                                                      const llvm::Loop *)>;

  /**
   * Store the all the loop invariant inputs.
   */
  std::unordered_set<const llvm::Value *> LoopInvariantInputs;

  /**
   * After all streams are created, this function should be called on every
   * stream to create the graphs.
   */
  void constructGraph(GetStreamFuncT GetStream);
  void addBaseStream(StaticStream *Other);
  void addBackEdgeBaseStream(StaticStream *Other);

  void computeBaseStepRootStreams();

  /**
   * We divide the detection of a stream by 3 stages:
   * 1. IsCandidate:
   *   A stream is candidate if it statisfies all the constraints that can be
   * determined without knowing the stream dependence graph, e.g. illegal
   * instruction in the datagraph, control dependencies, etc.
   *   This is set after constructing the MetaGraph
   *
   * 2. IsQualified:
   *   A stream is qualified if it is candidate and satisfies additional
   * constraints from the stream dependency graph, e.g. at most one step root
   * stream, base streams are qualified, etc.
   *   This is further divided into 2 sub-stages:
   *  a. checkIsQualifiedWithoutBackEdgeDep:
   *    The stream will check the base streams and mark itself qualified if all
   * the constraints are satisfied. Notice that in this stage the
   * StaticIndVarStream will ignore the back edge dependence.
   *  b. checkIsQualifiedWithBackEdgeDep:
   *    Since in the previous stage we ignore the back edge dependence, now it's
   * time to enforce it. This will mark some stream unqualified.
   *
   *  The reason why we need this is that pointer chase pattern forms a
   * dependence cycle. We have to first break the cycle by ingoring the back
   * edge dependence and propagate the qualified signal. And then enforce the
   * constraint by disqualify some streams.
   *
   * 3. IsChosen:
   *   A stream is chosen if it is qualified and the stream choice strategy
   * picks it up.
   *   This is set by the caller, e.g. region analyzer.
   *
   */
  virtual void analyzeIsCandidate() = 0;
  virtual bool checkIsQualifiedWithoutBackEdgeDep() const = 0;
  virtual bool checkIsQualifiedWithBackEdgeDep() const = 0;
  virtual void finalizePattern() = 0;
  bool isCandidate() const { return this->IsCandidate; }
  bool isQualified() const { return this->IsQualified; }
  void setIsQualified(bool IsQualified) {
    this->IsQualified = IsQualified;
    if (this->IsQualified) {
      this->StaticStreamInfo.clear_not_stream_reason();
      this->finalizePattern();
    }
  }
  bool isChosen() const { return this->IsChosen; }
  void markChosen() { this->IsChosen = true; }

  void setRegionStreamId(int Id) {
    assert(this->isChosen() && "Only chosen stream has RegionStreamId.");
    assert(this->RegionStreamId == -1 && "RegionStreamId set multiple times.");
    assert(Id >= 0 && "Illegal RegionStreamId set.");
    this->RegionStreamId = Id;
  }
  int getRegionStreamId() const { return this->RegionStreamId; }

  void setCoalesceGroup(uint64_t CoalesceGroup, int32_t CoalesceOffset = -1) {
    assert(this->CoalesceGroup == this->StreamId &&
           "Coalesce group is already set.");
    this->CoalesceGroup = CoalesceGroup;
    this->CoalesceOffset = CoalesceOffset;
  }
  int getCoalesceGroup() const { return this->CoalesceGroup; }

  /**
   * After mark chosen streams, we want to build the graph of chosen
   * streams.
   */
  virtual void constructChosenGraph();

  bool IsCandidate;
  bool IsQualified;
  bool IsChosen;
  /**
   * The unique id for streams configured at the same time.
   * Mainly for execution. Only chosen stream has this.
   */
  int RegionStreamId = -1;

  /**
   * CoalesceGroup. Default to myself with Offset 0.
   */
  uint64_t CoalesceGroup;
  int32_t CoalesceOffset;

  // Some bookkeeping information.
  mutable LLVM::TDG::StaticStreamInfo StaticStreamInfo;

  /**
   * Aliased MemStream.
   * This remembers the AliasBaseStream and offset to it.
   * AliasedStreams is sorted by increasing offset.
   * Note that aliased relationship is more general than coalesced
   * relationship. Coalesced also requires:
   * 1. Same BB.
   * 2. Should be load streams.
   * 3. No aliased store streams in between.
   */
  StaticStream *AliasBaseStream = nullptr;
  int64_t AliasOffset = 0;
  StreamVec AliasedStreams;

  // Update aliased stream.
  // It should be rare to have more than one update stream?
  StaticStream *UpdateStream = nullptr;

  // Predicate stream.
  BBPredicateDataGraph *BBPredDG = nullptr;
  StreamSet PredicatedTrueStreams;
  StreamSet PredicatedFalseStreams;

  // AddrDG for MemStream.
  std::unique_ptr<StreamDataGraph> AddrDG = nullptr;

  // Reduction stream.
  std::unique_ptr<StreamDataGraph> ReduceDG = nullptr;
  void generateAddrFunction(std::unique_ptr<llvm::Module> &Module) const;
  void generateReduceFunction(std::unique_ptr<llvm::Module> &Module) const;
  void generateValueFunction(std::unique_ptr<llvm::Module> &Module) const;

  // Store stream.
  std::unique_ptr<StreamDataGraph> ValueDG = nullptr;
  std::string getStoreFuncName(bool IsLoad) const {
    return this->FuncNameBase + (IsLoad ? "_load" : "_store");
  }
  using InputValueList = std::list<const llvm::Value *>;
  InputValueList getStoreFuncInputValues() const;
  InputValueList getReduceFuncInputValues() const;
  InputValueList getPredFuncInputValues() const;
  InputValueList getExecFuncInputValues(const ExecutionDataGraph &ExecDG) const;

  // Load fused ops.
  // So far only used for AtomicCmpXchg.
  using InstVec = std::vector<const llvm::Instruction *>;
  InstVec FusedLoadOps;
  void fuseLoadOps();

  /**
   * Stores all the input value for the analyzed pattern.
   * ! Only used this when the InputValuesValid is set.
   */
  bool InputValuesValid = false;
  std::list<const llvm::Value *> InputValues;

  using InstSet = std::unordered_set<const llvm::Instruction *>;
  const InstSet &getStepInsts() const { return this->StepInsts; }
  virtual const InstSet &getComputeInsts() const = 0;

  void fillProtobufStoreFuncInfoImpl(::LLVM::TDG::ExecFuncInfo *ExInfo,
                                     bool IsLoad) const;

  ::LLVM::TDG::DataType translateToProtobufDataType(llvm::Type *Type) const {
    return translateToProtobufDataType(this->DataLayout, Type);
  }
  static ::LLVM::TDG::DataType
  translateToProtobufDataType(llvm::DataLayout *DataLayout, llvm::Type *Type);

protected:
  static uint64_t AllocatedStreamId;
  static uint64_t allocateStreamId() {
    // Make sure 0 is reserved as invalid stream id.
    return ++AllocatedStreamId;
  }

  struct MetaNode {
    enum TypeE {
      PHIMetaNodeEnum,
      ComputeMetaNodeEnum,
    };
    TypeE Type;
  };

  struct ComputeMetaNode;
  struct PHIMetaNode : public MetaNode {
    PHIMetaNode(const llvm::PHINode *_PHINode) : PHINode(_PHINode) {
      this->Type = PHIMetaNodeEnum;
    }
    std::unordered_set<ComputeMetaNode *> ComputeMetaNodes;
    const llvm::PHINode *PHINode;
  };

  struct ComputeMetaNode : public MetaNode {
    ComputeMetaNode(const llvm::Value *_RootValue, const llvm::SCEV *_SCEV)
        : RootValue(_RootValue), SCEV(_SCEV) {
      this->Type = ComputeMetaNodeEnum;
    }
    std::unordered_set<PHIMetaNode *> PHIMetaNodes;
    StreamSet LoadBaseStreams;
    StreamSet IndVarBaseStreams;
    std::unordered_set<const llvm::Instruction *> CallInputs;
    std::unordered_set<const llvm::Value *> LoopInvariantInputs;
    std::vector<const llvm::Instruction *> ComputeInsts;
    /**
     * Root (final result) of this meta node.
     */
    const llvm::Value *RootValue;
    /**
     * Represent the scalar evolution of the result value.
     * nullptr if this is not SCEVable.
     */
    const llvm::SCEV *SCEV;
    bool isEmpty() const;
    bool isIdenticalTo(const ComputeMetaNode *Other) const;
    std::string format() const {
      return Utils::formatLLVMValueWithoutFunc(this->RootValue);
    }
  };
  std::unordered_set<const llvm::Instruction *> CallInputs;

  StreamSet LoadBaseStreams;
  StreamSet IndVarBaseStreams;

  std::list<PHIMetaNode> PHIMetaNodes;
  std::list<ComputeMetaNode> ComputeMetaNodes;

  struct DFSNode {
    ComputeMetaNode *ComputeMNode;
    const llvm::Value *Value;
    int VisitTimes;
    DFSNode(ComputeMetaNode *_ComputeMNode, const llvm::Value *_Value)
        : ComputeMNode(_ComputeMNode), Value(_Value), VisitTimes(0) {}
  };

  using ConstructedPHIMetaNodeMapT =
      std::unordered_map<const llvm::PHINode *, PHIMetaNode *>;
  using ConstructedComputeMetaNodeMapT =
      std::unordered_map<const llvm::Value *, ComputeMetaNode *>;

  ConstructedPHIMetaNodeMapT ConstructedPHIMetaNodeMap;
  ConstructedComputeMetaNodeMapT ConstructedComputeMetaNodeMap;

  /**
   * Remember the StepInsts.
   */
  InstSet StepInsts;

  /**
   * Handle the newly created PHIMetaNode.
   * Notice: this may modify DFSStack!
   */
  void handleFirstTimeComputeNode(
      std::list<DFSNode> &DFSStack, DFSNode &DNode,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap,
      GetStreamFuncT GetStream);

  /**
   * Handle the newly created ComputeMetaNode.
   * Notice: this may modify DFSStack!
   */
  void handleFirstTimePHIMetaNode(
      std::list<DFSNode> &DFSStack, PHIMetaNode *PHIMNode,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap);

  /**
   * Initialize the DFSStack for MetaGraph construction.
   */
  virtual void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) = 0;

  void constructMetaGraph(GetStreamFuncT GetStream);
  void constructStreamGraph();

  /**
   * Helper function to check that all base streams' InnerMostLoop contains
   * my InnerMostLoop.
   */
  bool checkBaseStreamInnerMostLoopContainsMine() const;

  /**
   * Helper function to check that there exists a static mapping from base
   * stream's element in a parent loop to my element, i.e. base stream's
   * InnerMostLoop > my InnerMostLoop.
   *
   * This basically requires that:
   * 1. such base stream is unconditionally stepped.
   * 2. I am also unconditionally stepped.
   * 3. The ratio between my step times and the base stream's is static, i.e.
   * the base stream steps every N times I step.
   *
   * Notice that this should only be checked when all the base streams are
   * qualified, because only qualified streams knows the StepPattern.
   */
  bool checkStaticMapFromBaseStreamInParentLoop() const;

  /**
   * Compute the step pattern. Should only be called when all base streams are
   * qualified.
   */
  virtual LLVM::TDG::StreamStepPattern computeStepPattern() const = 0;

  /**
   * Analyze if this is conditional access stream.
   */
  void analyzeIsConditionalAccess() const;

  /**
   * Analyze if this has known trip count.
   */
  void analyzeIsTripCountFixed() const;

  /**
   * This is a hack function to judge if this is a StepInst.
   * So far we just rely on the operation.
   */
  static bool isStepInst(const llvm::Instruction *Inst) {
    auto Opcode = Inst->getOpcode();
    switch (Opcode) {
    case llvm::Instruction::Add:
    case llvm::Instruction::GetElementPtr:
    case llvm::Instruction::Select: {
      return true;
    }
    default: {
      return false;
    }
    }
  }

  const StaticStream *getExecFuncInputStream(const llvm::Value *Value) const;
  void fillProtobufExecFuncInfo(::LLVM::TDG::ExecFuncInfo *ProtoFuncInfo,
                                const std::string &FuncName,
                                const ExecutionDataGraph &ExecDG,
                                llvm::Type *RetType) const;
  void fillProtobufAddrFuncInfo(::LLVM::TDG::ExecFuncInfo *AddrFuncInfo) const;
  void fillProtobufPredFuncInfo(::LLVM::TDG::ExecFuncInfo *PredFuncInfo) const;
  void fillProtobufStoreFuncInfo(::LLVM::TDG::StaticStreamInfo *SSInfo) const;
};
#endif