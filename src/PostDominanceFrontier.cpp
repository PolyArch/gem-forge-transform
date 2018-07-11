#include "PostDominanceFrontier.h"

PostDominanceFrontier::PostDominanceFrontier(
    const llvm::PostDominatorTree &Tree) {
  for (auto Root : Tree.getRoots()) {
    this->calculate(Tree, Tree.getNode(Root));
  }
}

const PostDominanceFrontier::SetT &
PostDominanceFrontier::calculate(const llvm::PostDominatorTree &Tree,
                                 const NodeT *Node) {
  auto BB = Node->getBlock();
  auto Iter = this->BBFrontierMap.find(BB);
  if (Iter == this->BBFrontierMap.end()) {
    // Create the frontier set.
    auto &Frontier =
        this->BBFrontierMap
            .emplace(std::piecewise_construct, std::forward_as_tuple(BB),
                     std::forward_as_tuple())
            .first->second;
    // Compute DF_local.
    for (auto Pred : llvm::predecessors(BB)) {
      auto PredNode = Tree.getNode(BB);
      if (PredNode->getIDom() != Node) {
        Frontier.insert(Pred);
      }
    }
    // Recursively compute DF_up
    for (auto ChildIter = Node->begin(), ChildEnd = Node->end();
         ChildIter != ChildEnd; ++ChildIter) {
      auto Child = *ChildIter;
      const auto &ChildFrontier = this->calculate(Tree, Child);
      for (auto ChildFrontierBB : ChildFrontier) {
        if (!Tree.properlyDominates(Node, Tree.getNode(ChildFrontierBB))) {
          Frontier.insert(ChildFrontierBB);
        }
      }
    }
    return Frontier;
  }
  return Iter->second;
}

const PostDominanceFrontier::SetT &
PostDominanceFrontier::getFrontier(llvm::BasicBlock *BB) const {
  auto Iter = this->BBFrontierMap.find(BB);
  assert(Iter != this->BBFrontierMap.end() &&
         "Missing frontier set for basic block.");
  return Iter->second;
}

CachedPostDominanceFrontier::~CachedPostDominanceFrontier() {
  for (auto &Entry : this->FuncToFrontierMap) {
    delete Entry.second;
    Entry.second = nullptr;
  }
  this->FuncToFrontierMap.clear();
}

const PostDominanceFrontier *
CachedPostDominanceFrontier::getPostDominanceFrontier(llvm::Function *Func) {
  auto Iter = this->FuncToFrontierMap.find(Func);
  if (Iter == this->FuncToFrontierMap.end()) {
    auto Frontier = new PostDominanceFrontier(this->GetTree(*Func));
    this->FuncToFrontierMap.emplace(Func, Frontier);
    return Frontier;
  }
  return Iter->second;
}