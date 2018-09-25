#ifndef LLVM_TDG_LOOP_UTILS_H
#define LLVM_TDG_LOOP_UTILS_H

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/Dominators.h"

#include <functional>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

/**
 * Determine if the static inst is the head of a loop.
 */
// inline bool isStaticInstLoopHead(llvm::Loop *Loop,
//                                  llvm::Instruction *StaticInst) {
//   assert(Loop != nullptr && "Null loop for isStaticInstLoopHead.");
//   return (Loop->getHeader() == StaticInst->getParent()) &&
//          (StaticInst->getParent()->getFirstNonPHI() == StaticInst);
// }

std::string printLoop(const llvm::Loop *Loop);

class LoopUtils {
public:
  /**
   * Returns true if a loop is continuous, i.e. once entered, the execution will
   * never falls out of the loop before the iteration ends.
   * This means it contains no function calls.
   */
  static bool isLoopContinuous(const llvm::Loop *Loop);

  /**
   * Get a global id for this loop.
   * Func::Header
   */
  static std::string getLoopId(const llvm::Loop *Loop);

  /**
   * Determine if the static inst is the head of a loop.
   */
  static bool isStaticInstLoopHead(llvm::Loop *Loop,
                                   llvm::Instruction *StaticInst);

  /**
   * Count the number of instructions in a loop, excluding PHINodes.
   */
  static uint64_t getNumStaticInstInLoop(llvm::Loop *Loop);

  /**
   * Print an static instruction.
   */
  static size_t getLLVMInstPosInBB(const llvm::Instruction *Inst);
  static std::string formatLLVMInst(const llvm::Instruction *Inst);
  static std::string formatLLVMValue(const llvm::Value *Value);

  static const std::unordered_set<std::string> SupportedMathFunctions;
};

class LoopIterCounter {
public:
  enum Status {
    SAME_ITER, // We are still in the previous iteration.
    NEW_ITER,  // We just entered a new iteration.
    OUT,       // We just jumped outside the loop.
  };

  LoopIterCounter() : Loop(nullptr) {}

  void configure(llvm::Loop *_Loop);
  void reset() {
    this->Loop = nullptr;
    this->Iter = -1;
  }
  bool isConfigured() const { return this->Loop != nullptr; }

  llvm::Loop *getLoop() { return this->Loop; }

  /**
   * Returns the status after considering the new static instruction.
   * Also returns the iteration count BEFORE the new static instruction.
   * Example,
   * 1. We are in iteration 3, and StaticInst is the header, it will return
   *    NEW_ITER, with Iter set to 3 (just completed iter 3).
   * 2. We are in iteration 4, and StaticInst is nullptr/outside inst, it will
   *    return OUT, with Iter set to 4 (just completed iter 4).
   * 3. We are in iteration 2, and StaticInst is in loop body (except the
   * header), it will return SAME_ITER, with Iter set to 2.
   */
  Status count(llvm::Instruction *StaticInst, int &Iter);

private:
  llvm::Loop *Loop;
  int Iter;
};

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
  using GetTLIFunc = std::function<llvm::TargetLibraryInfo &()>;
  using GetACFunc =
      std::function<llvm::AssumptionCache &(llvm::Function &Func)>;

  CachedLoopInfo(GetTLIFunc _GetTLI, GetACFunc _GetAC)
      : GetTLI(_GetTLI), GetAC(_GetAC) {}
  ~CachedLoopInfo();

  llvm::LoopInfo *getLoopInfo(llvm::Function *Func);
  llvm::DominatorTree *getDominatorTree(llvm::Function *Func);
  llvm::ScalarEvolution *getScalarEvolution(llvm::Function *Func);

private:
  std::unordered_map<llvm::Function *, llvm::LoopInfo *> LICache;
  std::unordered_map<llvm::Function *, llvm::DominatorTree *> DTCache;
  std::unordered_map<llvm::Function *, llvm::ScalarEvolution *> SECache;

  GetTLIFunc GetTLI;
  GetACFunc GetAC;
};

#endif