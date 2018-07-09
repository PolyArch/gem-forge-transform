#ifndef LLVM_TDG_LOOP_UTILS_H
#define LLVM_TDG_LOOP_UTILS_H

#include "llvm/Analysis/LoopInfo.h"

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
 * When ModulePass requires FunctionPass, each time the getAnalysis<>(F) will
 * rerun the analysis and override the previous result.
 * In order to improve performance, we cache some information of loop in this
 * class to reduce calls to getAnalysis<>(F).
 */
class StaticInnerMostLoop {
public:
  explicit StaticInnerMostLoop(llvm::Loop *Loop);
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

private:
  /**
   * Sort all the basic blocks in topological order and store the result in
   * BBList.
   */
  void scheduleBasicBlocksInLoop(llvm::Loop *Loop);
};

/**
 * This class represent a cached loop info.
 * This is useful as our transformation pass is module pass while loop info
 * is function pass. We have to cache the information.
 * This maintains a map from basic block to its inner most loop, if any.
 *
 * User need to provide a way for it to get LoopInfo from getAnlysis.
 * User can pass in additional lambda function to filter some inner most loops
 * if they do not care about.
 */
class CachedLoopInfo {
public:
  using GetLoopInfoT = std::function<llvm::LoopInfo &(llvm::Function &)>;
  using CareAboutT = std::function<bool(llvm::Loop *)>;
  CachedLoopInfo(GetLoopInfoT _GetLoopInfo, CareAboutT _CareAbout);
  ~CachedLoopInfo();

  /**
   * Get the cached StaticInnerMostLoop for that BB.
   * Returns nullptr if the basic block is contained within an inner most
   * loop we cared about.
   * If BB's function hasn't been analyzed, we will first call
   * getAnalysis<LoopInfoWrapperPass>(F) first to get the information. This
   * means that this functions should only be called within runOnXX functions.
   */
  StaticInnerMostLoop *getStaticLoopForBB(llvm::BasicBlock *BB);

private:
  GetLoopInfoT GetLoopInfo;
  CareAboutT CareAbout;

  /**
   * This set contains functions we have already cached the StaticInnerMostLoop.
   */
  std::unordered_set<llvm::Function *> FunctionsWithCachedStaticLoop;

  /**
   * Maps the bb to its inner most loop. This speeds up how we
   * decide when we hitting the header of a loop in the trace stream.
   */
  std::unordered_map<llvm::BasicBlock *, StaticInnerMostLoop *>
      BBToStaticLoopMap;

  void buildStaticLoopsIfNecessary(llvm::Function *Function);
};

#endif