#include "LoopUtils.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "LoopUtils"
/**
 * Determine if the static inst is the head of a loop.
 */
bool isStaticInstLoopHead(llvm::Loop *Loop, llvm::Instruction *StaticInst) {
  assert(Loop != nullptr && "Null loop for isStaticInstLoopHead.");
  return (Loop->getHeader() == StaticInst->getParent()) &&
         (StaticInst->getParent()->getFirstNonPHI() == StaticInst);
}

std::string printLoop(llvm::Loop *Loop) {
  return std::string(Loop->getHeader()->getParent()->getName()) +
         "::" + std::string(Loop->getName());
}

StaticInnerMostLoop::StaticInnerMostLoop(llvm::Loop *_Loop)
    : StaticInstCount(0), Loop(_Loop) {
  assert(Loop->empty() && "Should be inner most loops.");
  this->scheduleBasicBlocksInLoop(this->Loop);
  assert(!this->BBList.empty() && "Empty loops.");

  for (auto BB : this->BBList) {
    auto PHIs = BB->phis();
    this->StaticInstCount +=
        BB->size() - std::distance(PHIs.begin(), PHIs.end());
  }

  // Create the load/store order map.
  uint32_t LoadStoreIdx = 0;
  for (auto BB : this->BBList) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      if (llvm::isa<llvm::LoadInst>(Inst) || llvm::isa<llvm::StoreInst>(Inst)) {
        this->LoadStoreOrderMap.emplace(Inst, LoadStoreIdx++);
      }
    }
  }

  // Compute the live in and out.
  this->computeLiveInOutValues(this->Loop);
}

llvm::Instruction *StaticInnerMostLoop::getHeaderNonPhiInst() {
  if (this->BBList.empty()) {
    return nullptr;
  }
  return this->BBList.front()->getFirstNonPHI();
}

void StaticInnerMostLoop::scheduleBasicBlocksInLoop(llvm::Loop *Loop) {
  assert(Loop != nullptr && "Null Loop for scheduleBasicBlocksInLoop.");

  DEBUG(llvm::errs() << "Schedule basic blocks in loop " << printLoop(Loop)
                     << '\n');

  auto &Schedule = this->BBList;

  std::list<std::pair<llvm::BasicBlock *, bool>> Stack;
  std::unordered_set<llvm::BasicBlock *> Visited;
  std::unordered_set<llvm::BasicBlock *> Scheduled;

  Stack.emplace_back(Loop->getHeader(), false);
  while (!Stack.empty()) {
    auto &Entry = Stack.back();
    auto BB = Entry.first;
    if (Entry.second) {
      // This is the second time we visit BB.
      // We can schedule it.
      if (Scheduled.find(BB) == Scheduled.end()) {
        Schedule.push_front(BB);
        Scheduled.insert(BB);
        DEBUG(llvm::errs() << "Schedule " << BB->getName() << '\n');
      }
      Stack.pop_back();
      continue;
    }
    // This is the first time we visit BB, do DFS.
    Entry.second = true;
    Visited.insert(BB);
    for (auto Succ : llvm::successors(BB)) {
      if (!Loop->contains(Succ)) {
        continue;
      }
      if (Visited.find(Succ) != Visited.end()) {
        continue;
      }
      DEBUG(llvm::errs() << "Explore " << Succ->getName() << '\n');
      Stack.emplace_back(Succ, false);
    }
  }

  return;
}

void StaticInnerMostLoop::computeLiveInOutValues(llvm::Loop *Loop) {
  for (auto BB : this->BBList) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
           OperandIdx != NumOperands; ++OperandIdx) {
        auto Op = Inst->getOperand(OperandIdx);
        if (auto OpInst = llvm::dyn_cast<llvm::Instruction>(Op)) {
          // This is a register value.
          if (Loop->contains(OpInst)) {
            // This value is generated with in the loop.
            continue;
          }
        }
        // This value is either a global value, function parameter, or outside
        // instruction.
        this->LiveIns.insert(Op);
      }

      // Check the user of this inst.
      for (auto User : Inst->users()) {
        if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
          if (!Loop->contains(UserInst)) {
            this->LiveOuts.insert(UserInst);
          }
        } else {
          assert(false && "Other user than instructions?");
        }
      }
    }
  }
}

CachedLoopInfo::~CachedLoopInfo() {
  // Release the cached static loops.
  for (auto &Entry : this->LICache) {
    delete Entry.second;
  }
  this->LICache.clear();
  for (auto &Entry : this->DTCache) {
    delete Entry.second;
  }
  this->DTCache.clear();
}

llvm::LoopInfo *CachedLoopInfo::getLoopInfo(llvm::Function *Func) {
  auto Iter = this->LICache.find(Func);
  if (Iter == this->LICache.end()) {
    Iter = this->LICache
               .emplace(Func, new llvm::LoopInfo(*this->getDominatorTree(Func)))
               .first;
  }
  return Iter->second;
}

llvm::DominatorTree *CachedLoopInfo::getDominatorTree(llvm::Function *Func) {
  auto Iter = this->DTCache.find(Func);
  if (Iter == this->DTCache.end()) {
    auto DT = new llvm::DominatorTree();
    DT->recalculate(*Func);
    Iter = this->DTCache.emplace(Func, DT).first;
  }
  return Iter->second;
}

#undef DEBUG_TYPE