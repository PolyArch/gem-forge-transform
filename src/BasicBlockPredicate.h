#ifndef LLVM_TDG_BASIC_BLOCK_PREDICATE_H
#define LLVM_TDG_BASIC_BLOCK_PREDICATE_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>

/**
 * Most limited analysis of predication relationship between basic blocks.
 * Analyze the "predication data graph" (PredDG) of a BB, which is defined as:
 * 1. May only depend on load in the same BB, or value invariant of certain
 *    loop.
 * 2. Has two different TrueBB and FalseBB, which are dominated by BB and
 *    executed depending on the result of PredDG.
 */

class BBPredicateDataGraph {
public:
  BBPredicateDataGraph(const llvm::Loop *_Loop, const llvm::BasicBlock *_BB);
  BBPredicateDataGraph(const BBPredicateDataGraph &Other) = delete;
  BBPredicateDataGraph(BBPredicateDataGraph &&Other) = delete;
  BBPredicateDataGraph &operator=(const BBPredicateDataGraph &Other) = delete;
  BBPredicateDataGraph &operator=(BBPredicateDataGraph &&Other) = delete;

  bool isValid() const { return this->IsValid; }
  const llvm::BasicBlock *getTrueBB() const { return this->TrueBB; }
  const llvm::BasicBlock *getFalseBB() const { return this->FalseBB; }
  const std::list<const llvm::Value *> &getInputs() const {
    return this->Inputs;
  }
  const std::unordered_set<const llvm::LoadInst *> &getInputLoads() const {
    return this->InputLoads;
  }
  const std::unordered_set<const llvm::ConstantData *> &
  getConstantData() const {
    return this->ConstantDatas;
  }
  const std::unordered_set<const llvm::Instruction *> &getComputeInsts() const {
    return this->ComputeInsts;
  }

private:
  const llvm::Loop *Loop;
  const llvm::BasicBlock *BB;
  bool IsValid = false;
  const llvm::BasicBlock *TrueBB = nullptr;
  const llvm::BasicBlock *FalseBB = nullptr;
  std::list<const llvm::Value *> Inputs;
  std::unordered_set<const llvm::LoadInst *> InputLoads;
  std::unordered_set<const llvm::PHINode *> InputPHIs;
  std::unordered_set<const llvm::ConstantData *> ConstantDatas;
  std::unordered_set<const llvm::Instruction *> ComputeInsts;

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