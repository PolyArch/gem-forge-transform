#ifndef LLVM_TDG_CALL_LOOP_PROFILE_TREE_H
#define LLVM_TDG_CALL_LOOP_PROFILE_TREE_H

#include "../LoopUtils.h"

#include <iostream>
#include <memory>
#include <vector>

/**
 * This implements the CallLoopProfileTree in the paper:
 * J. Lau, E. Perelman and B. Calder,
 * "Selecting software phase markers with code structure analysis,"
 * International Symposium on Code Generation and Optimization (CGO'06)
 *
 * It takes in one instruction at a time and build up the tree.
 */

class CallLoopProfileTree {
public:
  using InstPtr = llvm::Instruction *;
  struct Edge {
    InstPtr StartInst;
    InstPtr BridgeInst;
    InstPtr DestInst;
    std::vector<uint64_t> InstCount;
    uint64_t SumInstCount = 0;
    uint64_t AvgInstCount = 0;
    uint64_t MaxInstCount = 0;
    double VarianceCoefficient = 0;
    bool Selected = false;
    // Unique id for this edge. Only valid if Selected is true.
    int SelectedID = 0;
    Edge(InstPtr _StartInst, InstPtr _BridgeInst, InstPtr _DestInst)
        : StartInst(_StartInst), BridgeInst(_BridgeInst), DestInst(_DestInst) {}
    void aggregateStats();
    void dump(int Tab, std::ostream &O) const;
  };
  using EdgePtr = std::shared_ptr<Edge>;
  struct Node {
    /**
     * First instruction of function/loop.
     */
    InstPtr Inst;
    Edge RecursiveEdge;
    std::vector<EdgePtr> InEdges;
    std::vector<EdgePtr> OutEdges;
    Node(InstPtr _Inst) : Inst(_Inst), RecursiveEdge(_Inst, _Inst, _Inst) {}
    EdgePtr getOutEdge(InstPtr BridgeInst, InstPtr DestInst);
    int EstimatedMaxDepth = 0;
    void aggregateStats();
    void dump(int Tab, std::ostream &O) const;
  };

  struct EdgeTraversePoint {
    EdgePtr E;
    const uint64_t TraverseInstCount;
    uint64_t TraverseMarkCount = 0;
    bool Selected = false;
    EdgeTraversePoint(EdgePtr _E, uint64_t _TraverseInstCount)
        : E(_E), TraverseInstCount(_TraverseInstCount) {}
  };

  CallLoopProfileTree(CachedLoopInfo *_CachedLI);

  CallLoopProfileTree(const CallLoopProfileTree &) = delete;
  CallLoopProfileTree &operator=(const CallLoopProfileTree &) = delete;
  CallLoopProfileTree(CallLoopProfileTree &&) = delete;
  CallLoopProfileTree &operator=(CallLoopProfileTree &&) = delete;

  void addInstruction(InstPtr Inst);
  void aggregateStats();
  void dump(std::ostream &O) const;

  void selectEdges();
  const std::vector<EdgeTraversePoint> &getSelectedEdgeTimeline() const {
    return this->SelectedEdgeTimeline;
  }
  const std::vector<EdgePtr> &getSelectedEdges() const {
    return this->SelectedEdges;
  }

private:
  CachedLoopInfo *CachedLI;

  Node RootNode;
  /**
   * Map from RegionInst to Nodes.
   */
  std::unordered_map<InstPtr, Node> RegionInstNodeMap;
  /**
   * Remember the timeline of edges.
   */
  std::vector<EdgeTraversePoint> EdgeTimeline;

  /**
   * Transient states during building the tree.
   */
  InstPtr PrevInst = nullptr;
  // Remember a stack of regions.
  struct RegionStackFrame {
    Node *N;
    uint64_t FirstEnterInstCount;
    uint64_t RecurEnterInstCount;
    InstPtr BridgeInst;
    RegionStackFrame(Node *_N, uint64_t _EnterInstCount, InstPtr _BridgeInst)
        : N(_N), FirstEnterInstCount(_EnterInstCount),
          RecurEnterInstCount(_EnterInstCount), BridgeInst(_BridgeInst) {}
  };
  std::vector<RegionStackFrame> RegionStack;
  uint64_t GlobalInstCount = 0;

  InstPtr getRegionInst(InstPtr Inst);
  Node *getOrAllocateNode(InstPtr RegionInst);
  EdgePtr getOrAllocateEdge(Node *FromNode, Node *ToNode, InstPtr BridgeInst);

  std::vector<EdgePtr> CandidateEdges;
  double AvgCovOfCandidateEdges = 0;
  double StdCovOfCandidateEdges = 0;
  std::vector<EdgePtr> SelectedEdges;
  std::vector<EdgeTraversePoint> SelectedEdgeTimeline;

  void estimateNodeMaxDepth();
  void selectCandidateEdges();
};

#endif