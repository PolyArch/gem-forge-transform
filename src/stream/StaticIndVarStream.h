#ifndef LLVM_TDG_STREAM_STATIC_IND_VAR_STREAM_H
#define LLVM_TDG_STREAM_STATIC_IND_VAR_STREAM_H

#include "stream/StaticStream.h"

class StaticIndVarStream : public StaticStream {
public:
  StaticIndVarStream(llvm::PHINode *_PHINode, const llvm::Loop *_ConfigureLoop,
                     const llvm::Loop *_InnerMostLoop,
                     llvm::ScalarEvolution *_SE,
                     const llvm::PostDominatorTree *_PDT,
                     llvm::DataLayout *_DataLayout)
      : StaticStream(TypeT::IV, _PHINode, _ConfigureLoop, _InnerMostLoop, _SE,
                     _PDT, _DataLayout),
        PHINode(_PHINode), NonEmptyComputePath(nullptr) {}

  bool checkIsQualifiedWithoutBackEdgeDep() const override;
  bool checkIsQualifiedWithBackEdgeDep() const override;
  void finalizePattern() override;
  const InstSet &getComputeInsts() const override { return this->ComputeInsts; }

  llvm::PHINode *PHINode;

private:
  struct ComputePath {
    std::vector<ComputeMetaNode *> ComputeMetaNodes;
    bool isEmpty() const {
      if (this->ComputeMetaNodes.empty()) {
        return true;
      }
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
            Other->ComputeMetaNodes.at(ComputeMetaNodeIdx);
        if (!ThisComputeMNode->isIdenticalTo(OtherComputeMNode)) {
          return false;
        }
      }
      return true;
    }
    void debug() const;
  };

  /**
   * Initialize the DFSStack by pushing the first PHINodeMetaGraph.
   */
  void initializeMetaGraphConstruction(
      std::list<DFSNode> &DFSStack,
      ConstructedPHIMetaNodeMapT &ConstructedPHIMetaNodeMap,
      ConstructedComputeMetaNodeMapT &ConstructedComputeMetaNodeMap) override;

  void constructMetaGraph(GetStreamFuncT GetStream) override;

  void analyzeValuePattern() override;

  void analyzeIsCandidate() override;

  std::list<ComputePath> AllComputePaths;
  const ComputePath *NonEmptyComputePath;
  bool EmptyComputePathFound = false;

  InstSet ComputeInsts;

  std::list<ComputePath> constructComputePath() const;

  void
  constructComputePathRecursive(ComputeMetaNode *ComputeMNode,
                                ComputePath &CurrentPath,
                                std::list<ComputePath> &AllComputePaths) const;

  /**
   * Analyze the pattern from the SCEV.
   * If it's random, it also sets the not_stream_reason.
   */
  LLVM::TDG::StreamValuePattern analyzeValuePatternFromComputePath(
      const ComputeMetaNode *FirstNonEmptyComputeMNode);

  /**
   * Analyze if this a simple reduction stream for load.
   */
  bool analyzeIsReductionFromComputePath(
      const ComputeMetaNode *FirstNonEmptyComputeMNode) const;

  /**
   * Analyze if this is a simple pointer-chase stream for a single load.
   */
  bool analyzeIsPointerChaseFromComputePath(
      const ComputeMetaNode *FirstNonEmptyComputeMNode) const;

  /**
   * Build the ReduceDG.
   */
  void buildReduceDG(const ComputeMetaNode *FirstNonEmptyComputeMNode);

  /**
   * Check if ReduceDG is complete.
   */
  bool checkReduceDGComplete();

  LLVM::TDG::StreamStepPattern computeStepPattern() const override {
    // No computation required.
    return this->StaticStreamInfo.stp_pattern();
  }

  /**
   * Do a BFS on the PHINode and extract all the step instructions.
   * This is actually legacy code from trace simulation, but I kept
   * it here.
   */
  void searchStepInsts();

  /**
   * Do a BFS on the PHINode and extract all the compute instructions.
   * This is actually legacy code from trace simulation, but
   */
  void searchComputeInsts();
};
#endif