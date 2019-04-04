#ifndef LLVM_TDG_STREAM_STATIC_STREAM_H
#define LLVM_TDG_STREAM_STATIC_STREAM_H

#include "LoopUtils.h"
#include "Utils.h"

#include "stream/StreamMessage.pb.h"

#include <unordered_set>

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

  StaticStream(TypeT _Type, const llvm::Instruction *_Inst,
               const llvm::Loop *_ConfigureLoop,
               const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE)
      : Type(_Type), Inst(_Inst), ConfigureLoop(_ConfigureLoop),
        InnerMostLoop(_InnerMostLoop), SE(_SE), IsCandidate(false),
        IsQualified(false), IsStream(false),
        AccPattern(LLVM::TDG::StreamAccessPattern::UNKNOWN),
        ValPattern(LLVM::TDG::StreamValuePattern::RANDOM) {}
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
  virtual void buildDependenceGraph(GetStreamFuncT GetStream) = 0;
  void addBaseStream(StaticStream *Other) {
    this->BaseStreams.insert(Other);
    if (Other != nullptr) {
      Other->DependentStreams.insert(this);
      if (Other->InnerMostLoop == this->InnerMostLoop) {
        this->BaseStepStreams.insert(Other);
      }
    }
  }
  void addBackEdgeBaseStream(StaticStream *Other) {
    this->BackMemBaseStreams.insert(Other);
    Other->BackIVDependentStreams.insert(this);
  }
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
   *   This is set after constructing the StreamDependenceGraph. The value
   * pattern and access pattern are also set in this stage. The manager is in
   * charged of propagating the qualified signal.
   *
   * 3. IsChosen:
   *   A stream is chosen if it is qualified and the stream choice strategy
   * picks it up.
   *   This is set by the caller, e.g. region analyzer.
   *
   */

  bool IsCandidate;
  bool IsQualified;
  bool IsStream;
  LLVM::TDG::StreamAccessPattern AccPattern;
  LLVM::TDG::StreamValuePattern ValPattern;

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
    std::unordered_set<const llvm::LoadInst *> LoadInputs;
    std::unordered_set<const llvm::Instruction *> CallInputs;
    std::unordered_set<const llvm::Value *> LoopInvarianteInputs;
    std::unordered_set<const llvm::PHINode *> LoopHeaderPHIInputs;
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
  std::unordered_set<const llvm::LoadInst *> LoadInputs;
  std::unordered_set<const llvm::Instruction *> CallInputs;
  std::unordered_set<const llvm::Value *> LoopInvarianteInputs;
  std::unordered_set<const llvm::PHINode *> LoopHeaderPHIInputs;

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

  /**
   * Handle the newly created PHIMetaNode.
   * Notice: this may modify DFSStack!
   */
  void handleFirstTimeComputeNode(
      std::list<DFSNode> &DFSStack, DFSNode &DNode,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap);

  /**
   * Handle the newly created ComputeMetaNode.
   * Notice: this may modify DFSStack!
   */
  void handleFirstTimePHIMetaNode(
      std::list<DFSNode> &DFSStack, PHIMetaNode *PHIMNode,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap);

  /**
   * Construct the MetaGraph.
   */
  void constructMetaGraph();

  /**
   * Initialize the DFSStack for MetaGraph construction.
   */
  virtual void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) = 0;
};

#endif