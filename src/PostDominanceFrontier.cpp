#include "PostDominanceFrontier.h"

#define DEBUG_TYPE "PostDominanceFrontier"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

PostDominanceFrontier::PostDominanceFrontier(
    const llvm::PostDominatorTree &Tree) {
  LLVM_DEBUG(Tree.print(llvm::errs()));
  auto RootNode = Tree.getRootNode();
  if (RootNode->getBlock() == nullptr) {
    // There are multiple exit nodes. Explicitly handle this by process the
    // second level nodes.
    for (auto ChildIter = RootNode->begin(), ChildEnd = RootNode->end();
         ChildIter != ChildEnd; ++ChildIter) {
      this->calculate(Tree, *ChildIter);
    }
  } else {
    // Single valid exit node.
    this->calculate(Tree, RootNode);
  }
  LLVM_DEBUG(this->print(llvm::errs()));
}

const PostDominanceFrontier::SetT &
PostDominanceFrontier::calculate(const llvm::PostDominatorTree &Tree,
                                 const NodeT *Node) {
  auto BB = Node->getBlock();
  auto Iter = this->BBFrontierMap.find(BB);
  if (Iter == this->BBFrontierMap.end()) {
    // Create the frontier set.
    LLVM_DEBUG(llvm::errs() << "Calculate for BB " << BB->getName() << '\n');
    auto &Frontier =
        this->BBFrontierMap
            .emplace(std::piecewise_construct, std::forward_as_tuple(BB),
                     std::forward_as_tuple())
            .first->second;
    // Compute DF_local.
    for (auto Pred : llvm::predecessors(BB)) {
      auto PredNode = Tree.getNode(Pred);
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

void PostDominanceFrontier::print(llvm::raw_ostream &O) const {
  for (const auto &Iter : this->BBFrontierMap) {
    O << "BB " << Iter.first->getName() << ": \n";
    for (const auto BB : Iter.second) {
      O << "  " << BB->getName() << ", ";
    }
    O << '\n';
  }
}

const PostDominanceFrontier::SetT &
PostDominanceFrontier::getFrontier(llvm::BasicBlock *BB) const {
  auto Iter = this->BBFrontierMap.find(BB);
  if (Iter == this->BBFrontierMap.end()) {
    LLVM_DEBUG(llvm::errs()
               << "Missing frontier set for basic block "
               << BB->getParent()->getName() << "::" << BB->getName() << '\n');
  }
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
    auto Frontier =
        new PostDominanceFrontier(*this->getPostDominatorTree(Func));
    this->FuncToFrontierMap.emplace(Func, Frontier);
    return Frontier;
  }
  return Iter->second;
}

const llvm::PostDominatorTree *
CachedPostDominanceFrontier::getPostDominatorTree(llvm::Function *Func) {
  auto Iter = this->FuncToTreeMap.find(Func);
  if (Iter == this->FuncToTreeMap.end()) {
    llvm::PostDominatorTree *PDT = new llvm::PostDominatorTree();
    PDT->recalculate(*Func);
    this->FuncToTreeMap.emplace(Func, PDT);
    return PDT;
  }
  return Iter->second;
}

#undef DEBUG_TYPE