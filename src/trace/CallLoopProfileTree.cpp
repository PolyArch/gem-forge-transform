#include "CallLoopProfileTree.h"
#include "../Utils.h"

#include <cmath>

CallLoopProfileTree::CallLoopProfileTree(CachedLoopInfo *_CachedLI)
    : CachedLI(_CachedLI), RootNode(nullptr) {
  this->RegionStack.emplace_back(&this->RootNode, this->GlobalInstCount,
                                 this->PrevInst);
}

void CallLoopProfileTree::addInstruction(InstPtr Inst) {

  // Get the new RegionInst.
  auto RegionInst = this->getRegionInst(Inst);

  auto PrevNode = this->RegionStack.back().N;
  auto &PrevRegionFirstEnterInstCount =
      this->RegionStack.back().FirstEnterInstCount;
  auto &PrevRegionRecurEnterInstCount =
      this->RegionStack.back().RecurEnterInstCount;
  auto PrevRegionInst = PrevNode->Inst;

  if (RegionInst == PrevRegionInst) {
    // Still in the same region.
    if (Inst == RegionInst) {
      // Recursion happened.
      auto InstCount = GlobalInstCount - PrevRegionRecurEnterInstCount;
      PrevNode->RecursiveEdge.InstCount.push_back(InstCount);
      PrevRegionRecurEnterInstCount = GlobalInstCount;
    } else {
      // Just keep counting (do nothing).
    }
  } else {
    // Different region.
    auto Node = this->getOrAllocateNode(RegionInst);
    if (Inst == RegionInst) {
      // This should be a new region.
      // Add an edge and push to region stack.
      auto E = this->getOrAllocateEdge(PrevNode, Node, this->PrevInst);
      this->RegionStack.emplace_back(Node, this->GlobalInstCount,
                                     this->PrevInst);
      this->EdgeTimeline.emplace_back(E, this->GlobalInstCount);
    } else {
      // This should be an old region.
      // Pop RegionStack.
      while (this->RegionStack.size() > 1 &&
             this->RegionStack.back().N != Node) {
        // Pop an edge out.
        auto ToNode = this->RegionStack.back().N;
        auto InstCount = this->GlobalInstCount -
                         this->RegionStack.back().FirstEnterInstCount;
        auto BridgeInst = this->RegionStack.back().BridgeInst;
        // Pop this region.
        this->RegionStack.pop_back();
        auto FromNode = this->RegionStack.back().N;
        // Find the edge.
        auto E = FromNode->getOutEdge(BridgeInst, ToNode->Inst);
        E->InstCount.push_back(InstCount);
      }
      if (this->RegionStack.back().N != Node) {
        // We still failed to find the region. Add one node under root.
        assert(this->RegionStack.size() == 1);
        assert(this->RegionStack.back().N == &this->RootNode);
        auto E = this->getOrAllocateEdge(&this->RootNode, Node,
                                         nullptr /* BridgeInst */);
        this->RegionStack.emplace_back(Node, this->GlobalInstCount,
                                       nullptr /* BridgeInst */);
        this->EdgeTimeline.emplace_back(E, this->GlobalInstCount);
      }
    }
  }

  // Finally remember as the PrevInst.
  this->PrevInst = Inst;
  this->GlobalInstCount++;
  if (this->GlobalInstCount % 10000000 == 0) {
    llvm::dbgs() << "Added " << this->GlobalInstCount / 1000000
                 << " million insts.\n";
  }
}

CallLoopProfileTree::InstPtr CallLoopProfileTree::getRegionInst(InstPtr Inst) {
  auto Func = Inst->getFunction();
  auto LI = this->CachedLI->getLoopInfo(Func);
  auto Loop = LI->getLoopFor(Inst->getParent());
  if (Loop) {
    return &Loop->getHeader()->front();
  } else {
    return &Func->front().front();
  }
}

CallLoopProfileTree::Node *
CallLoopProfileTree::getOrAllocateNode(InstPtr RegionInst) {
  return &this->RegionInstNodeMap.emplace(RegionInst, RegionInst).first->second;
}

std::shared_ptr<CallLoopProfileTree::Edge>
CallLoopProfileTree::getOrAllocateEdge(Node *FromNode, Node *ToNode,
                                       InstPtr BridgeInst) {
  for (auto &Edge : FromNode->OutEdges) {
    if (Edge->BridgeInst == BridgeInst && Edge->DestInst == ToNode->Inst) {
      return Edge;
    }
  }
  auto NewEdge =
      std::make_shared<Edge>(FromNode->Inst, BridgeInst, ToNode->Inst);
  FromNode->OutEdges.push_back(NewEdge);
  ToNode->InEdges.push_back(NewEdge);
  return NewEdge;
}

void CallLoopProfileTree::aggregateStats() {
  for (auto &InstNode : this->RegionInstNodeMap) {
    auto &Node = InstNode.second;
    Node.aggregateStats();
  }
}

void CallLoopProfileTree::Edge::aggregateStats() {
  this->SumInstCount = 0;
  this->AvgInstCount = 0;
  this->MaxInstCount = 0;
  this->VarianceCoefficient = 0;
  for (auto Count : this->InstCount) {
    this->SumInstCount += Count;
    this->MaxInstCount = std::max(this->MaxInstCount, Count);
  }
  if (this->SumInstCount > 0) {
    this->AvgInstCount = this->SumInstCount / this->InstCount.size();
    double Variance = 0;
    for (auto Count : this->InstCount) {
      double Diff =
          static_cast<double>(Count) - static_cast<double>(this->AvgInstCount);
      Variance += Diff * Diff;
    }
    Variance /= static_cast<double>(this->InstCount.size());
    Variance = sqrt(Variance);
    this->VarianceCoefficient =
        Variance / static_cast<double>(this->AvgInstCount) * 100.;
  }
}

void CallLoopProfileTree::Node::aggregateStats() {
  for (auto &E : this->OutEdges) {
    E->aggregateStats();
  }
  this->RecursiveEdge.aggregateStats();
}

void CallLoopProfileTree::Edge::dump(int Tab, std::ostream &O) const {
  for (int I = 0; I < Tab; ++I) {
    O << ' ';
  }
  O << "x " << this->InstCount.size() << " Avg " << this->AvgInstCount
    << " Max " << this->MaxInstCount << " Cov " << this->VarianceCoefficient
    << ' ';
  if (this->StartInst == this->DestInst) {
    O << "Recursive";
  } else {
    O << "Dest " << Utils::formatLLVMInst(this->DestInst);
  }
  O << '\n';
}

void CallLoopProfileTree::Node::dump(int Tab, std::ostream &O) const {
  for (int I = 0; I < Tab; ++I) {
    O << ' ';
  }
  if (this->Inst) {
    O << "Node " << Utils::formatLLVMInst(this->Inst);
  } else {
    O << "Root";
  }
  O << " Depth " << this->EstimatedMaxDepth << '\n';
  this->RecursiveEdge.dump(Tab + 2, O);
  for (const auto &E : this->OutEdges) {
    E->dump(Tab + 2, O);
  }
}

void CallLoopProfileTree::dump(std::ostream &O) const {
  this->RootNode.dump(0, O);
  std::unordered_set<llvm::Function *> DumpedFunctions;
  for (const auto &InstNode : this->RegionInstNodeMap) {
    auto Inst = InstNode.first;
    auto Func = Inst->getFunction();
    if (DumpedFunctions.count(Func)) {
      continue;
    }
    {
      // Dump the function.
      auto FuncEnterInst = &Func->front().front();
      if (this->RegionInstNodeMap.count(FuncEnterInst)) {
        this->RegionInstNodeMap.at(FuncEnterInst).dump(0, O);
      }
      // Dump all loops.
      auto LI = this->CachedLI->getLoopInfo(Func);
      for (auto Loop : LI->getLoopsInPreorder()) {
        auto LoopEnterInst = &Loop->getHeader()->front();
        if (this->RegionInstNodeMap.count(LoopEnterInst)) {
          this->RegionInstNodeMap.at(LoopEnterInst)
              .dump(Loop->getLoopDepth() * 2, O);
        }
      }
    }
    DumpedFunctions.insert(Func);
  }
  O << "------------- Candidate Edges ---------------\n";
  O << "Avg Cov " << this->AvgCovOfCandidateEdges << " Std Cov "
    << this->StdCovOfCandidateEdges << '\n';
  for (const auto &E : this->CandidateEdges) {
    O << "  ";
    if (E->StartInst) {
      O << Utils::formatLLVMInst(E->StartInst) << " ->";
    } else {
      O << "Root ->";
    }
    E->dump(2, O);
  }
  O << "------------- Selected Edges ----------------\n";
  for (const auto &E : this->SelectedEdges) {
    O << "  ";
    if (E->StartInst) {
      O << Utils::formatLLVMInst(E->StartInst) << " ->";
    } else {
      O << "Root ->";
    }
    E->dump(2, O);
  }
  O << "------------- Selected Intervals ------------\n";
  uint64_t PrevIntervalPoint = 0;
  uint64_t IntervalIdx = 0;
  for (const auto &Point : this->SelectedEdgeTimeline) {
    O << "Interval " << IntervalIdx << " " << Point.TraverseInstCount << " +"
      << Point.TraverseInstCount - PrevIntervalPoint;
    if (Point.Selected) {
      O << " Selected\n";
    } else {
      O << '\n';
    }
    PrevIntervalPoint = Point.TraverseInstCount;
    IntervalIdx++;
  }
}

std::shared_ptr<CallLoopProfileTree::Edge>
CallLoopProfileTree::Node::getOutEdge(InstPtr BridgeInst, InstPtr DestInst) {
  for (auto &E : this->OutEdges) {
    if (E->DestInst == DestInst && E->BridgeInst == BridgeInst) {
      return E;
    }
  }
  assert(false && "Failed to find Edge.");
  return nullptr;
}

void CallLoopProfileTree::selectEdges() {
  this->estimateNodeMaxDepth();
  this->selectCandidateEdges();
}

void CallLoopProfileTree::estimateNodeMaxDepth() {
  std::unordered_map<Node *, int> VisitedMap;
  std::vector<Node *> DFSStack;
  DFSStack.push_back(&this->RootNode);
  VisitedMap.emplace(&this->RootNode, 0);
  while (!DFSStack.empty()) {
    auto N = DFSStack.back();
    auto &Visited = VisitedMap.at(N);
    if (Visited == 0) {
      // First time, Push in children.
      auto EstimatedDepth = N->EstimatedMaxDepth + 1;
      for (const auto &E : N->OutEdges) {
        auto OutN = &this->RegionInstNodeMap.at(E->DestInst);
        if (OutN->EstimatedMaxDepth >= EstimatedDepth) {
          continue;
        }
        auto &OutVisited = VisitedMap.emplace(OutN, 0).first->second;
        if (OutVisited == 1) {
          // This one is on the current path. Ignore it.
          continue;
        } else {
          // We push it to the stack anyway.
          OutVisited = 0;
          DFSStack.push_back(OutN);
          OutN->EstimatedMaxDepth = EstimatedDepth;
        }
      }
      Visited = 1;
    } else if (Visited == 1) {
      // We are done with it.
      DFSStack.pop_back();
      Visited = 2;
    } else if (Visited == 2) {
      // Someone already processed.
      DFSStack.pop_back();
    }
  }
}

void CallLoopProfileTree::selectCandidateEdges() {
  // Sort all nodes with decreasing depth.
  std::vector<Node *> SortedNodes;
  for (auto &InstNode : this->RegionInstNodeMap) {
    SortedNodes.push_back(&InstNode.second);
  }
  std::sort(SortedNodes.begin(), SortedNodes.end(),
            [](const Node *A, const Node *B) -> bool {
              if (A->EstimatedMaxDepth != B->EstimatedMaxDepth) {
                return A->EstimatedMaxDepth > B->EstimatedMaxDepth;
              } else if (A->OutEdges.size() != B->OutEdges.size()) {
                return A->OutEdges.size() < B->OutEdges.size();
              } else {
                auto AInstUID = Utils::getInstUIDMap().getUID(A->Inst);
                auto BInstUID = Utils::getInstUIDMap().getUID(B->Inst);
                return AInstUID < BInstUID;
              }
            });
  this->CandidateEdges.clear();
  for (auto &N : SortedNodes) {
    // Processing all incoming edges.
    for (auto &E : N->InEdges) {
      if (E->AvgInstCount < MinimumInstCount) {
        continue;
      }
      this->CandidateEdges.push_back(E);
    }
  }
  // Let's sort with SumInstCount.
  std::sort(this->CandidateEdges.begin(), this->CandidateEdges.end(),
            [](const EdgePtr &A, const EdgePtr &B) -> bool {
              return A->SumInstCount > B->SumInstCount;
            });

  // We also calculate the Cov.
  double SumCov = 0;
  for (const auto &E : this->CandidateEdges) {
    SumCov += E->VarianceCoefficient;
  }
  this->AvgCovOfCandidateEdges = SumCov / this->CandidateEdges.size();
  this->StdCovOfCandidateEdges = 0;
  for (const auto &E : this->CandidateEdges) {
    auto CovDiff = this->AvgCovOfCandidateEdges - E->VarianceCoefficient;
    this->StdCovOfCandidateEdges += CovDiff * CovDiff;
  }
  this->StdCovOfCandidateEdges =
      sqrt(this->StdCovOfCandidateEdges / this->CandidateEdges.size());

  /**
   * So far we simply select edges that below AvgCov + StdCov, with a valid
   * Start/Bridge/DestInst.
   */
  this->SelectedEdges.clear();
  for (auto &E : this->CandidateEdges) {
    if (E->VarianceCoefficient >
        this->AvgCovOfCandidateEdges + this->StdCovOfCandidateEdges) {
      continue;
    }
    if (!E->BridgeInst || !E->StartInst || !E->DestInst) {
      continue;
    }
    this->SelectedEdges.push_back(E);
    E->Selected = true;
    E->SelectedID = this->SelectedEdges.size();
  }

  // Select interval points now.
  this->SelectedEdgeTimeline.clear();
  for (const auto &TraversePoint : this->EdgeTimeline) {
    if (TraversePoint.E->Selected) {
      this->SelectedEdgeTimeline.emplace_back(TraversePoint.E,
                                              TraversePoint.TraverseInstCount);
      this->SelectedEdgeTimeline.back().TraverseMarkCount =
          this->SelectedEdgeTimeline.size() - 1;
    }
  }

  // Merge intervals that are too small.
  uint64_t PrevSelectedPointInstCount = 0;
  for (uint64_t Idx = 0; Idx < this->SelectedEdgeTimeline.size(); ++Idx) {
    auto &Point = this->SelectedEdgeTimeline.at(Idx);
    if (Idx == 0) {
      // We always select the first one.
      Point.Selected = true;
      PrevSelectedPointInstCount = Point.TraverseInstCount;
    } else {
      // Select if the interval is large enough.
      if (Point.TraverseInstCount - PrevSelectedPointInstCount >=
          MinimumInstCount) {
        Point.Selected = true;
        PrevSelectedPointInstCount = Point.TraverseInstCount;
      }
    }
  }
}