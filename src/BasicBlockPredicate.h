#ifndef LLVM_TDG_BASIC_BLOCK_PREDICATE_H
#define LLVM_TDG_BASIC_BLOCK_PREDICATE_H

#include "ExecutionDataGraph.h"

/**
 * Most limited analysis of predication relationship between basic blocks.
 * Analyze the "predication data graph" (PredDG) of a BB, which is defined as:
 * 1. May only depend on load in the same BB, or value invariant of certain
 *    loop.
 * 2. Has two different TrueBB and FalseBB, which are dominated by BB and
 *    executed depending on the result of PredDG.
 */

class BBPredicateDataGraph : public ExecutionDataGraph {
public:
  BBPredicateDataGraph(const llvm::Loop *_Loop, const llvm::BasicBlock *_BB);
  BBPredicateDataGraph(const BBPredicateDataGraph &Other) = delete;
  BBPredicateDataGraph(BBPredicateDataGraph &&Other) = delete;
  BBPredicateDataGraph &operator=(const BBPredicateDataGraph &Other) = delete;
  BBPredicateDataGraph &operator=(BBPredicateDataGraph &&Other) = delete;

  ~BBPredicateDataGraph() {}

  const std::string &getFuncName() const { return this->FuncName; }
  bool isValid() const { return this->IsValid; }
  const llvm::BasicBlock *getTrueBB() const { return this->TrueBB; }
  const llvm::BasicBlock *getFalseBB() const { return this->FalseBB; }
  const llvm::BasicBlock *getTrueLoopHeaderBB() const {
    return this->TrueLoopHeaderBB;
  }
  const llvm::BasicBlock *getFalseLoopHeaderBB() const {
    return this->FalseLoopHeaderBB;
  }
  const std::unordered_set<const llvm::LoadInst *> &getInputLoads() const {
    return this->InputLoads;
  }

private:
  const llvm::Loop *Loop;
  const llvm::BasicBlock *BB;
  const std::string FuncName;
  bool IsValid = false;
  const llvm::BasicBlock *TrueBB = nullptr;
  const llvm::BasicBlock *FalseBB = nullptr;
  const llvm::BasicBlock *TrueLoopHeaderBB = nullptr;
  const llvm::BasicBlock *FalseLoopHeaderBB = nullptr;
  std::unordered_set<const llvm::LoadInst *> InputLoads;
  std::unordered_set<const llvm::PHINode *> InputPHIs;

  void constructDataGraph();
};

class CachedBBPredicateDataGraph {
public:
  CachedBBPredicateDataGraph() {}
  CachedBBPredicateDataGraph(const CachedBBPredicateDataGraph &Other) = delete;
  CachedBBPredicateDataGraph(CachedBBPredicateDataGraph &&Other) = delete;
  CachedBBPredicateDataGraph &
  operator=(const CachedBBPredicateDataGraph &Other) = delete;
  CachedBBPredicateDataGraph &
  operator=(CachedBBPredicateDataGraph &&Other) = delete;
  ~CachedBBPredicateDataGraph();

  BBPredicateDataGraph *getBBPredicateDataGraph(const llvm::Loop *Loop,
                                                const llvm::BasicBlock *BB);
  BBPredicateDataGraph *tryBBPredicateDataGraph(const llvm::Loop *Loop,
                                                const llvm::BasicBlock *BB);

private:
  using Key = std::pair<const llvm::Loop *, const llvm::BasicBlock *>;
  struct KeyHasher {
    std::size_t operator()(const Key &K) const {
      return reinterpret_cast<std::size_t>(K.first) ^
             reinterpret_cast<std::size_t>(K.second);
    }
  };
  std::unordered_map<Key, BBPredicateDataGraph *, KeyHasher> KeyToDGMap;
};

#endif