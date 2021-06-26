#ifndef LLVM_TDG_BASIC_BLOCK_BRANCH_DATA_GRAPH_H
#define LLVM_TDG_BASIC_BLOCK_BRANCH_DATA_GRAPH_H

#include "ExecutionDataGraph.h"

/**
 * This serves as the basic case for a branch data graph:
 * 1. May only depend on loads in the same BB, or value invariant of
 *    certain loop.
 * 2. Has two different TrueBB and FalseBB.
 */

class BBBranchDataGraph : public ExecutionDataGraph {
public:
  BBBranchDataGraph(const llvm::Loop *_Loop, const llvm::BasicBlock *_BB,
                    const std::string _Suffix = "_br");
  BBBranchDataGraph(const BBBranchDataGraph &Other) = delete;
  BBBranchDataGraph(BBBranchDataGraph &&Other) = delete;
  BBBranchDataGraph &operator=(const BBBranchDataGraph &Other) = delete;
  BBBranchDataGraph &operator=(BBBranchDataGraph &&Other) = delete;

  ~BBBranchDataGraph() {}

  const std::string &getFuncName() const { return this->FuncName; }
  bool isValid() const { return this->IsValid; }
  const llvm::BasicBlock *getTrueBB() const { return this->TrueBB; }
  const llvm::BasicBlock *getFalseBB() const { return this->FalseBB; }
  const std::unordered_set<const llvm::LoadInst *> &getInputLoads() const {
    return this->InputLoads;
  }

  /******************************************************************
   * This are additional checks for certain DataGraph requirement.
   * NOTE: TargetBB must be either TrueBB/FalseBB.
   ******************************************************************/

  /**
   * Predication requires:
   * 1. True/FalseBB are not this BB.
   * 2. True/FalseBB are dominated by this BB.
   */
  bool isValidPredicate() const {
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
   * 3. Or other Predecessor is from inside of the Loop.
   */
  bool isValidLoopHeadPredicate() const {
    return this->isLoopHeadPredicate(this->TrueBB) ||
           this->isLoopHeadPredicate(this->FalseBB);
  }
  const llvm::BasicBlock *getLoopHeadPredicateBB(bool True) const {
    auto TargetBB = True ? this->TrueBB : this->FalseBB;
    return this->isLoopHeadPredicate(TargetBB) ? TargetBB : nullptr;
  }
  bool isLoopHeadPredicate(const llvm::BasicBlock *TargetBB) const;

protected:
  const llvm::Loop *Loop;
  const llvm::BasicBlock *BB;
  const std::string FuncName;
  bool IsValid = false;
  const llvm::BasicBlock *TrueBB = nullptr;
  const llvm::BasicBlock *FalseBB = nullptr;
  std::unordered_set<const llvm::LoadInst *> InputLoads;
  std::unordered_set<const llvm::PHINode *> InputPHIs;

  void constructDataGraph();
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

  BBBranchDataGraph *getBBBranchDataGraph(const llvm::Loop *Loop,
                                          const llvm::BasicBlock *BB);
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