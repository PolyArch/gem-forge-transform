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
    Edge(InstPtr _StartInst, InstPtr _BridgeInst, InstPtr _DestInst)
        : StartInst(_StartInst), BridgeInst(_BridgeInst), DestInst(_DestInst) {}
    void aggregateStats();
    void dump(int Tab, std::ostream &O) const;
  };
  struct Node {
    /**
     * First instruction of function/loop.
     */
    InstPtr Inst;
    Edge RecursiveEdge;
    std::vector<std::shared_ptr<Edge>> InEdges;
    std::vector<std::shared_ptr<Edge>> OutEdges;
    Node(InstPtr _Inst) : Inst(_Inst), RecursiveEdge(_Inst, _Inst, _Inst) {}
    std::shared_ptr<Edge> getOutEdge(InstPtr BridgeInst, InstPtr DestInst);
    int EstimatedMaxDepth = 0;
    void aggregateStats();
    void dump(int Tab, std::ostream &O) const;
  };

  struct EdgeTraversePoint {
    std::shared_ptr<Edge> E;
    const uint64_t TraverseInstCount;
    uint64_t TraverseMarkCount = 0;
    bool Selected = false;
    EdgeTraversePoint(std::shared_ptr<Edge> _E, uint64_t _TraverseInstCount)
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

  const std::vector<EdgeTraversePoint> &selectEdges();

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
  std::shared_ptr<Edge> getOrAllocateEdge(Node *FromNode, Node *ToNode,
                                          InstPtr BridgeInst);

  std::vector<std::shared_ptr<Edge>> CandidateEdges;
  double AvgCovOfCandidateEdges = 0;
  double StdCovOfCandidateEdges = 0;
  std::vector<std::shared_ptr<Edge>> SelectedEdges;
  std::vector<EdgeTraversePoint> SelectedEdgeTimeline;

  static constexpr uint64_t MinimumInstCount = 10000000;

  void estimateNodeMaxDepth();
  void selectCandidateEdges();
};

#endif