#ifndef LLVM_TDG_STREAM_STATIC_STREAM_H
#define LLVM_TDG_STREAM_STATIC_STREAM_H

#include "StreamUtils.h"

#include "LoopUtils.h"
#include "Utils.h"

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
  enum TypeT {
    IV,
    MEM,
  };
  const TypeT Type;
  const llvm::Instruction *const Inst;
  const llvm::Loop *const ConfigureLoop;
  const llvm::Loop *const InnerMostLoop;
  llvm::ScalarEvolution *const SE;

  /**
   * The constructor just creates the object and does not perform any analysis.
   *
   * After creating all the streams, the manager should call constructGraph() to
   * initialize the MetaGraph and StreamGraph.
   */
  StaticStream(TypeT _Type, const llvm::Instruction *_Inst,
               const llvm::Loop *_ConfigureLoop,
               const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE)
      : Type(_Type), Inst(_Inst), ConfigureLoop(_ConfigureLoop),
        InnerMostLoop(_InnerMostLoop), SE(_SE), IsCandidate(false),
        IsQualified(false), IsStream(false) {}
  virtual ~StaticStream() {}
  void setStaticStreamInfo(LLVM::TDG::StaticStreamInfo &SSI) const;

  std::string formatType() const {
    switch (this->Type) {
    case StaticStream::IV:
      return "IV";
    case StaticStream::MEM:
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
           this->ConfigureLoop->getHeader()->getName().str() + " " +
           Utils::formatLLVMInstWithoutFunc(this->Inst) + ")";
  }

  /**
   * This represents the bidirectional dependence graph between streams.
   */
  using StreamSet = std::unordered_set<StaticStream *>;
  StreamSet BaseStreams;
  StreamSet DependentStreams;
  StreamSet BackMemBaseStreams;
  StreamSet BackIVDependentStreams;
  StreamSet BaseStepStreams;
  StreamSet BaseStepRootStreams;

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

  bool IsCandidate;
  bool IsQualified;
  bool IsStream;
  // Some bookkeeping information.
  mutable LLVM::TDG::StaticStreamInfo StaticStreamInfo;

  /**
   * Stores all the input value for the analyzed pattern.
   * ! Only used this when the InputValuesValid is set.
   */
  bool InputValuesValid = false;
  std::list<const llvm::Value *> InputValues;

protected:
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
    bool isEmpty() const {
      /**
       * Check if this ComputeMNode does nothing.
       * So far we just check that there is no compute insts.
       * Further we can allow something like binary extension.
       */
      return this->ComputeInsts.empty();
    }
    bool isIdenticalTo(const ComputeMetaNode *Other) const;
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
};
#endif