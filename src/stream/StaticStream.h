#ifndef LLVM_TDG_STREAM_STATIC_STREAM_H
#define LLVM_TDG_STREAM_STATIC_STREAM_H

#include "LoopUtils.h"
#include "Utils.h"

#include "stream/StreamMessage.pb.h"

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
        InnerMostLoop(_InnerMostLoop), SE(_SE), IsStream(false),
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

  bool IsStream;
  LLVM::TDG::StreamAccessPattern AccPattern;
  LLVM::TDG::StreamValuePattern ValPattern;

protected:
};

class StaticMemStream : public StaticStream {
public:
  StaticMemStream(const llvm::Instruction *_Inst,
                  const llvm::Loop *_ConfigureLoop,
                  const llvm::Loop *_InnerMostLoop, llvm::ScalarEvolution *_SE)
      : StaticStream(TypeT::MEM, _Inst, _ConfigureLoop, _InnerMostLoop, _SE) {}
};

class StaticIndVarStream : public StaticStream {
public:
  StaticIndVarStream(llvm::PHINode *_PHINode,
                     const llvm::Loop *_ConfigureLoop,
                     const llvm::Loop *_InnerMostLoop,
                     llvm::ScalarEvolution *_SE)
      : StaticStream(TypeT::IV, _PHINode, _ConfigureLoop, _InnerMostLoop, _SE),
        PHINode(_PHINode) {
    this->constructMetaGraph();
    this->analyze();
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

  void analyze();

private:
  llvm::PHINode *PHINode;

  std::unordered_set<const llvm::LoadInst *> LoadInputs;
  std::unordered_set<const llvm::Instruction *> CallInputs;
  std::unordered_set<const llvm::Value *> LoopInvarianteInputs;
  std::unordered_set<const llvm::PHINode *> LoopHeaderPHIInputs;

  std::list<PHIMetaNode> PHIMetaNodes;
  std::list<ComputeMetaNode> ComputeMetaNodes;

  struct ComputePath {
    std::vector<ComputeMetaNode *> ComputeMetaNodes;
    bool isEmpty() const {
      for (const auto &ComputeMNode : this->ComputeMetaNodes) {
        if (!ComputeMNode->isEmpty()) {
          return false;
        }
      }
      return true;
    }
    bool isIdenticalTo(const ComputePath *Other) const {
      if (this->ComputeMetaNodes.size() != Other->ComputeMetaNodes.size()) {
        return false;
      }
      for (size_t ComputeMetaNodeIdx = 0,
                  NumComputeMetaNodes = this->ComputeMetaNodes.size();
           ComputeMetaNodeIdx != NumComputeMetaNodes; ++ComputeMetaNodeIdx) {
        const auto &ThisComputeMNode =
            this->ComputeMetaNodes.at(ComputeMetaNodeIdx);
        const auto &OtherComputeMNode =
            this->ComputeMetaNodes.at(ComputeMetaNodeIdx);
        if (!ThisComputeMNode->isIdenticalTo(OtherComputeMNode)) {
          return false;
        }
      }
      return true;
    }
    void debug() const;
  };

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

  void handleFirstTimeComputeNode(
      std::list<DFSNode> &DFSStack, DFSNode &DNode,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap);

  void handleFirstTimePHIMetaNode(
      std::list<DFSNode> &DFSStack, PHIMetaNode *PHIMNode,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap);

  void constructMetaGraph();

  std::list<ComputePath> constructComputePath() const;

  void
  constructComputePathRecursive(ComputeMetaNode *ComputeMNode,
                                ComputePath &CurrentPath,
                                std::list<ComputePath> &AllComputePaths) const;

  LLVM::TDG::StreamValuePattern
  analyzeValuePatternFromSCEV(const llvm::SCEV *SCEV) const;
};
#endif