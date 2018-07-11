#ifndef LLVM_TDG_POST_DOMINANCE_FRONTIER_H
#define LLVM_TDG_POST_DOMINANCE_FRONTIER_H

#include "llvm/Analysis/PostDominators.h"

#include <unordered_map>
#include <unordered_set>

class PostDominanceFrontier {
public:
  PostDominanceFrontier(const llvm::PostDominatorTree &Tree);

  using SetT = std::unordered_set<llvm::BasicBlock *>;
  const SetT &getFrontier(llvm::BasicBlock *BB) const;

private:
  std::unordered_map<llvm::BasicBlock *, SetT> BBFrontierMap;

  using NodeT = llvm::DomTreeNodeBase<llvm::BasicBlock>;
  const SetT &calculate(const llvm::PostDominatorTree &Tree, const NodeT *Node);
};

/**
 * This class stored cached dominance frontier for each function.
 */
class CachedPostDominanceFrontier {
public:
  using GetTreeT = std::function<llvm::PostDominatorTree &(llvm::Function &)>;

  CachedPostDominanceFrontier(GetTreeT _GetTree)
      : GetTree(std::move(_GetTree)) {}
  ~CachedPostDominanceFrontier();

  /**
   * Get the post dominance frontier for this function. If this is the first
   * time the function is queried, we will compute the frontier.
   */
  const PostDominanceFrontier *getPostDominanceFrontier(llvm::Function *Func);

private:
  std::unordered_map<llvm::Function *, PostDominanceFrontier *>
      FuncToFrontierMap;

  GetTreeT GetTree;
};

#endif