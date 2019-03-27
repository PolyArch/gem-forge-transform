#include "LoopUtils.h"
#include "Utils.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "LoopUtils"
/**
 * Determine if the static inst is the head of a loop.
 */
// bool isStaticInstLoopHead(const llvm::Loop *Loop,
//                           llvm::Instruction *StaticInst) {
//   assert(Loop != nullptr && "Null loop for isStaticInstLoopHead.");
//   return (Loop->getHeader() == StaticInst->getParent()) &&
//          (StaticInst->getParent()->getFirstNonPHI() == StaticInst);
// }

const std::unordered_set<std::string> LoopUtils::SupportedMathFunctions{
    "exp", "sin", "sqrt", "acos", "fabs", "abs", "rand", "log", "lgamma"};

std::string printLoop(const llvm::Loop *Loop) {
  return std::string(Loop->getHeader()->getParent()->getName()) +
         "::" + std::string(Loop->getName());
}

bool LoopUtils::isStaticInstLoopHead(llvm::Loop *Loop,
                                     llvm::Instruction *StaticInst) {
  assert(Loop != nullptr && "Null loop for isStaticInstLoopHead.");
  return (Loop->getHeader() == StaticInst->getParent()) &&
         (StaticInst->getParent()->getFirstNonPHI() == StaticInst);
}

bool LoopUtils::isLoopContinuous(const llvm::Loop *Loop) {
  // Check if there is any calls to unsupported function.
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
      if (!Utils::isCallOrInvokeInst(&*InstIter)) {
        continue;
      }
      auto *Callee = Utils::getCalledFunction(&*InstIter);
      if (Callee == nullptr) {
        // Indirect call, not continuous.
        DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
                           << " is statically not continuous because it "
                              "contains indirect call.\n");
        return false;
      }
      if (Callee->isIntrinsic()) {
        continue;
      }
      // Check if calling some supported math function.
      if (Callee->isDeclaration()) {
        if (LoopUtils::SupportedMathFunctions.count(Callee->getName()) != 0) {
          continue;
        }
      }
      DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
                         << " is statically not continuous because it "
                            "contains unsupported call to "
                         << Callee->getName() << "\n");
      return false;
    }
  }
  return true;
}

std::string LoopUtils::getLoopId(const llvm::Loop *Loop) {
  auto Header = Loop->getHeader();
  auto Func = Header->getParent();
  return Utils::formatLLVMFunc(Func) + "::" + Loop->getName().str();
}

uint64_t LoopUtils::getNumStaticInstInLoop(llvm::Loop *Loop) {
  uint64_t StaticInsts = 0;
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    auto BB = *BBIter;
    auto PHIs = BB->phis();
    StaticInsts += BB->size() - std::distance(PHIs.begin(), PHIs.end());
  }
  return StaticInsts;
}

int LoopUtils::countPossiblePath(const llvm::Loop *Loop) {
  std::unordered_set<const llvm::BasicBlock *> OnPathBBs;
  return LoopUtils::countPossiblePathFromBB(Loop, OnPathBBs, Loop->getHeader());
}

int LoopUtils::countPossiblePathFromBB(
    const llvm::Loop *Loop,
    std::unordered_set<const llvm::BasicBlock *> &OnPathBBs,
    const llvm::BasicBlock *CurrentBB) {
  if (!Loop->contains(CurrentBB)) {
    // Exiting the loop.
    return 1;
  }
  int Count = 0;
  OnPathBBs.insert(CurrentBB);
  for (const auto Succ : llvm::successors(CurrentBB)) {
    if (OnPathBBs.count(Succ)) {
      continue;
    }
    Count += LoopUtils::countPossiblePathFromBB(Loop, OnPathBBs, Succ);
  }
  OnPathBBs.erase(CurrentBB);
  return Count;
}

std::string LoopUtils::formatLLVMInst(const llvm::Instruction *Inst) {
  if (Inst->getName() != "") {
    return (llvm::Twine(Inst->getFunction()->getName()) +
            "::" + Inst->getParent()->getName() + "::" + Inst->getName() + "(" +
            Inst->getOpcodeName() + ")")
        .str();
  } else {
    size_t Idx = LoopUtils::getLLVMInstPosInBB(Inst);
    return (llvm::Twine(Inst->getFunction()->getName()) +
            "::" + Inst->getParent()->getName() + "::" + llvm::Twine(Idx) +
            "(" + Inst->getOpcodeName() + ")")
        .str();
  }
}

size_t LoopUtils::getLLVMInstPosInBB(const llvm::Instruction *Inst) {
  size_t Idx = 0;
  for (auto InstIter = Inst->getParent()->begin(),
            InstEnd = Inst->getParent()->end();
       InstIter != InstEnd; ++InstIter) {
    if ((&*InstIter) == Inst) {
      break;
    }
    Idx++;
  }
  return Idx;
}

std::string LoopUtils::formatLLVMValue(const llvm::Value *Value) {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    return LoopUtils::formatLLVMInst(Inst);
  } else {
    return Value->getName();
  }
}

llvm::Instruction *LoopUtils::getUnrollableTerminator(llvm::Loop *Loop) {

  // Basically copies from LoopUnroll.cpp

  auto Header = Loop->getHeader();
  if (Header == nullptr) {
    return nullptr;
  }

  auto LatchBlock = Loop->getLoopLatch();
  if (LatchBlock == nullptr) {
    return nullptr;
  }

  if (!Loop->isSafeToClone()) {
    return nullptr;
  }

  auto BI = llvm::dyn_cast<llvm::BranchInst>(LatchBlock->getTerminator());
  if (!BI || BI->isUnconditional()) {
    return nullptr;
  }

  auto CheckSuccessors = [&](unsigned S1, unsigned S2) {
    return BI->getSuccessor(S1) == Header &&
           !Loop->contains(BI->getSuccessor(S2));
  };

  if (!CheckSuccessors(0, 1) && !CheckSuccessors(1, 0)) {
    return nullptr;
  }

  return BI;
}

void LoopIterCounter::configure(llvm::Loop *_Loop) {
  this->Loop = _Loop;
  this->Iter = -1;
}

LoopIterCounter::Status LoopIterCounter::count(llvm::Instruction *StaticInst,
                                               int &Iter) {
  assert(this->isConfigured() && "LoopIterCounter is not configured.");
  if (StaticInst == nullptr || (!this->Loop->contains(StaticInst))) {
    // Outside instruction.
    Iter = this->Iter + 1;
    this->Iter = -1;
    return OUT;
  }
  if (LoopUtils::isStaticInstLoopHead(this->Loop, StaticInst)) {
    this->Iter++;
    Iter = this->Iter;
    return NEW_ITER;
  }
  Iter = this->Iter;
  return SAME_ITER;
}

StaticInnerMostLoop::StaticInnerMostLoop(llvm::Loop *_Loop)
    : StaticInstCount(0), Loop(_Loop) {
  assert(Loop->empty() && "Should be inner most loops.");
  this->scheduleBasicBlocksInLoop(this->Loop);
  assert(!this->BBList.empty() && "Empty loops.");

  this->StaticInstCount = LoopUtils::getNumStaticInstInLoop(this->Loop);

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
  for (auto &Entry : this->SECache) {
    delete Entry.second;
  }
  this->SECache.clear();

  // Release the cached static loops.
  for (auto &Entry : this->LICache) {
    delete Entry.second;
  }
  this->LICache.clear();
  for (auto &Entry : this->DTCache) {
    delete Entry.second;
  }
  this->DTCache.clear();
  for (auto &Entry : this->ACCache) {
    delete Entry.second;
  }
  this->ACCache.clear();

  delete this->TLI;
}

llvm::AssumptionCache *
CachedLoopInfo::getAssumptionCache(llvm::Function *Func) {
  auto Iter = this->ACCache.find(Func);
  if (Iter == this->ACCache.end()) {
    Iter = this->ACCache.emplace(Func, new llvm::AssumptionCache(*Func)).first;
  }
  return Iter->second;
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

llvm::ScalarEvolution *
CachedLoopInfo::getScalarEvolution(llvm::Function *Func) {
  auto Iter = this->SECache.find(Func);
  if (Iter == this->SECache.end()) {
    auto SE = new llvm::ScalarEvolution(
        *Func, *this->TLI, *this->getAssumptionCache(Func),
        *this->getDominatorTree(Func), *this->getLoopInfo(Func));
    Iter = this->SECache.emplace(Func, SE).first;
  }
  return Iter->second;
}

llvm::Instruction *CachedLoopInfo::getUnrollableTerminator(llvm::Loop *Loop) {
  auto Iter = this->UnrollableTerminatorCache.find(Loop);
  if (Iter == this->UnrollableTerminatorCache.end()) {
    Iter = this->UnrollableTerminatorCache
               .emplace(Loop, LoopUtils::getUnrollableTerminator(Loop))
               .first;
  }
  return Iter->second;
}

#undef DEBUG_TYPE