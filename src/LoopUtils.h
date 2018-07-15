#ifndef LLVM_TDG_LOOP_UTILS_H
#define LLVM_TDG_LOOP_UTILS_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"

#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

/**
 * Determine if the static inst is the head of a loop.
 */
bool isStaticInstLoopHead(llvm::Loop *Loop, llvm::Instruction *StaticInst);

std::string printLoop(llvm::Loop *Loop);

/**
 * A wrapper of llvm::Loop, but with more information calculated:
 *
 * 1. The schedule of basic blocks.
 * 2. The live in and live out values.
 * 3. Total number of static instructions.
 */
class StaticInnerMostLoop {
public:
  explicit StaticInnerMostLoop(llvm::Loop *_Loop);
  StaticInnerMostLoop(const StaticInnerMostLoop &Other) = delete;
  StaticInnerMostLoop(StaticInnerMostLoop &&Other) = delete;
  StaticInnerMostLoop &operator=(const StaticInnerMostLoop &Other) = delete;
  StaticInnerMostLoop &operator=(StaticInnerMostLoop &&Other) = delete;

  /**
   * Return the first non-phi inst in the header.
   */
  llvm::Instruction *getHeaderNonPhiInst();

  llvm::BasicBlock *getHeader() const { return this->BBList.front(); }

  /**
   * Contains the basic blocks in topological sorted order, where the first
   * one is the header.
   */
  std::list<llvm::BasicBlock *> BBList;

  /**
   * Store the order of memory access instructions in this loop.
   * This is used to implement relaxed memory dependence constraints.
   */
  std::unordered_map<llvm::Instruction *, uint32_t> LoadStoreOrderMap;

  std::string print() const {
    return std::string(this->getHeader()->getParent()->getName()) +
           "::" + std::string(this->getHeader()->getName());
  }

  size_t StaticInstCount;

  /**
   * Store the live in values when the loop started.
   */
  std::unordered_set<llvm::Value *> LiveIns;

  /**
   * Stores the value used outside the loop.
   */
  std::unordered_set<llvm::Instruction *> LiveOuts;

  /**
   * The original llvm loop.
   */
  llvm::Loop *Loop;

private:
  /**
   * Sort all the basic blocks in topological order and store the result in
   * BBList.
   */
  void scheduleBasicBlocksInLoop(llvm::Loop *Loop);

  /**
   * Identify live in and out values.
   */
  void computeLiveInOutValues(llvm::Loop *Loop);
};

/**
 * This class represent a cached loop info.
 * This is useful as our transformation pass is module pass while loop info
 * is function pass. We have to cache the information to avoid recalculation.
 * This maintains a map from basic block to its inner most loop, if any.
 *
 * Since LoopInfo is based on dominator tree, it also caches the dominator
 * tree.
 *
 */
class CachedLoopInfo {
public:
  CachedLoopInfo() = default;
  ~CachedLoopInfo();

  llvm::LoopInfo *getLoopInfo(llvm::Function *Func);
  llvm::DominatorTree *getDominatorTree(llvm::Function *Func);

private:
  std::unordered_map<llvm::Function *, llvm::LoopInfo *> LICache;
  std::unordered_map<llvm::Function *, llvm::DominatorTree *> DTCache;
};

#endif