#ifndef LLVM_TDG_STREAM_STATIC_IND_VAR_STREAM_H
#define LLVM_TDG_STREAM_STATIC_IND_VAR_STREAM_H

#include "stream/StaticStream.h"

class StaticIndVarStream : public StaticStream {
public:
  StaticIndVarStream(llvm::PHINode *_PHINode, const llvm::Loop *_ConfigureLoop,
                     const llvm::Loop *_InnerMostLoop,
                     llvm::ScalarEvolution *_SE)
      : StaticStream(TypeT::IV, _PHINode, _ConfigureLoop, _InnerMostLoop, _SE),
        PHINode(_PHINode), NonEmptyComputePath(nullptr) {}

  bool checkIsQualifiedWithoutBackEdgeDep() const override;
  bool checkIsQualifiedWithBackEdgeDep() const override;
  void finalizePattern() override;

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
            this->ComputeMetaNodes.at(ComputeMetaNodeIdx);
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

  void analyzeIsCandidate() override;

  std::list<ComputePath> AllComputePaths;
  const ComputePath *NonEmptyComputePath;

  std::list<ComputePath> constructComputePath() const;

  void
  constructComputePathRecursive(ComputeMetaNode *ComputeMNode,
                                ComputePath &CurrentPath,
                                std::list<ComputePath> &AllComputePaths) const;

  /**
   * Analyze the pattern from the SCEV.
   * If it's random, it also sets the not_stream_reason.
   */
  LLVM::TDG::StreamValuePattern
  analyzeValuePatternFromSCEV(const llvm::SCEV *SCEV) const;

  LLVM::TDG::StreamStepPattern computeStepPattern() const override {
    // No computation required.
    return this->StaticStreamInfo.stp_pattern();
  }
};
#endif