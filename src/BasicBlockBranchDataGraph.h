#ifndef LLVM_TDG_BASIC_BLOCK_BRANCH_DATA_GRAPH_H
#define LLVM_TDG_BASIC_BLOCK_BRANCH_DATA_GRAPH_H

#include "ExecutionDataGraph.h"

#include "llvm/Analysis/PostDominators.h"

/**
 * This serves as the basic case for a branch data graph:
 * 1. May only depend on loads in the same BB, or value invariant of
 *    certain loop.
 * 2. Has two different TrueBB and FalseBB.
 */

class BBBranchDataGraph : public ExecutionDataGraph {
public:
  using IsInputFuncT = std::function<bool(const llvm::Instruction *)>;
  BBBranchDataGraph(const llvm::Loop *_Loop, const llvm::BasicBlock *_BB,
                    IsInputFuncT _IsInput, const std::string _Suffix = "_br");
  BBBranchDataGraph(const BBBranchDataGraph &Other) = delete;
  BBBranchDataGraph(BBBranchDataGraph &&Other) = delete;
  BBBranchDataGraph &operator=(const BBBranchDataGraph &Other) = delete;
  BBBranchDataGraph &operator=(BBBranchDataGraph &&Other) = delete;

  ~BBBranchDataGraph() {}

  const std::string &getFuncName() const { return this->FuncName; }
  const llvm::Loop *getLoop() const { return this->Loop; }
  bool isValid() const { return this->IsValid; }
  const llvm::BasicBlock *getTrueBB() const { return this->TrueBB; }
  const llvm::BasicBlock *getFalseBB() const { return this->FalseBB; }
  const std::unordered_set<const llvm::Instruction *> &getInLoopInputs() const {
    return this->InLoopInputs;
  }

  /******************************************************************
   * This are additional checks for certain DataGraph requirement.
   * NOTE: TargetBB must be either TrueBB/FalseBB.
   ******************************************************************/

  /**
   * Predication requires:
   * 1. True/FalseBB are not this BB.
   * 2. True/FalseBB are dominated by this BB.
   * 3. All Inputs are from the same BB, or post-dominated by BB.
   */
  bool isValidPredicate(const llvm::PostDominatorTree *PDT) const {
    for (auto InputInst : this->InLoopInputs) {
      auto InputBB = InputInst->getParent();
      if (InputBB != this->BB && !PDT->dominates(this->BB, InputBB)) {
        return false;
      }
    }
    return this->isPredicate(this->TrueBB) || this->isPredicate(this->FalseBB);
  }
  const llvm::BasicBlock *getPredicateBB(bool True) const {
    auto TargetBB = True ? this->TrueBB : this->FalseBB;
    return this->isPredicate(TargetBB) ? TargetBB : nullptr;
  }
  bool isPredicate(const llvm::BasicBlock *TargetBB) const;

  /**
   * LoopHeaderBB requires:
   * 1. True/FalseBB are not this BB.
   * 2. TargetBB is a LoopHeader.
   * 3. All other Predecessor is from inside of the Loop.
   * NOTICE: We may extend TargetBB along the unconditional chain.
   * TargetBB -> BB -> LoopHeaderBB.
   */
  bool isValidLoopHeadPredicate() const {
    return this->getLoopHeadPredicateBB(this->TrueBB) ||
           this->getLoopHeadPredicateBB(this->FalseBB);
  }
  const llvm::BasicBlock *getLoopHeadPredicateBB(bool True) const {
    auto TargetBB = True ? this->TrueBB : this->FalseBB;
    return this->getLoopHeadPredicateBB(TargetBB);
  }
  /**
   * Check this BBPredDG forms a LoopHeadPredication.
   * Notice that we may try to expand TargetBB following the unconditional chain
   * to handle some loop-invariant code optimization.
   * @return Possibly extended TargetBB or nullptr.
   */
  const llvm::BasicBlock *
  getLoopHeadPredicateBB(const llvm::BasicBlock *TargetBB) const;

  /**
   * LoopBoundBB requires:
   * 1. This BB is the single latch/exiting of the Loop.
   * 2. The Loop has a single BB (is this too restrict?).
   */
  bool isValidLoopBoundPredicate() const;

protected:
  const llvm::Loop *Loop;
  const llvm::BasicBlock *BB;
  const std::string FuncName;
  bool IsValid = false;
  const llvm::BasicBlock *TrueBB = nullptr;
  const llvm::BasicBlock *FalseBB = nullptr;
  std::unordered_set<const llvm::Instruction *> InLoopInputs;

  void constructDataGraph(IsInputFuncT IsInput);
  void clear();
};

class CachedBBBranchDataGraph {
public:
  CachedBBBranchDataGraph() {}
  CachedBBBranchDataGraph(const CachedBBBranchDataGraph &Other) = delete;
  CachedBBBranchDataGraph(CachedBBBranchDataGraph &&Other) = delete;
  CachedBBBranchDataGraph &
  operator=(const CachedBBBranchDataGraph &Other) = delete;
  CachedBBBranchDataGraph &operator=(CachedBBBranchDataGraph &&Other) = delete;
  ~CachedBBBranchDataGraph();

  using IsInputFuncT = BBBranchDataGraph::IsInputFuncT;
  BBBranchDataGraph *getBBBranchDataGraph(const llvm::Loop *Loop,
                                          const llvm::BasicBlock *BB,
                                          IsInputFuncT IsInput);
  BBBranchDataGraph *tryBBBranchDataGraph(const llvm::Loop *Loop,
                                          const llvm::BasicBlock *BB);

private:
  using Key = std::pair<const llvm::Loop *, const llvm::BasicBlock *>;
  struct KeyHasher {
    std::size_t operator()(const Key &K) const {
      return reinterpret_cast<std::size_t>(K.first) ^
             reinterpret_cast<std::size_t>(K.second);
    }
  };
  std::unordered_map<Key, BBBranchDataGraph *, KeyHasher> KeyToDGMap;
};

#endif