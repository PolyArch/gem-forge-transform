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

StaticInnerMostLoop::StaticInnerMostLoop(llvm::Loop *Loop)
    : StaticInstCount(0) {
  assert(Loop->empty() && "Should be inner most loops.");
  this->scheduleBasicBlocksInLoop(Loop);
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
  this->computeLiveInOutValues(Loop);
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
  std::unordered_set<llvm::BasicBlock *> Scheduled;

  Stack.emplace_back(Loop->getHeader(), false);
  while (!Stack.empty()) {
    auto &Entry = Stack.back();
    auto BB = Entry.first;
    if (Scheduled.find(BB) != Scheduled.end()) {
      // This BB has already been scheduled.
      Stack.pop_back();
      continue;
    }
    if (Entry.second) {
      // This is the second time we visit BB.
      // We can schedule it.
      Schedule.push_front(BB);
      Scheduled.insert(BB);
      Stack.pop_back();
      continue;
    }
    // This is the first time we visit BB, do DFS.
    Entry.second = true;
    for (auto Succ : llvm::successors(BB)) {
      if (!Loop->contains(Succ)) {
        continue;
      }
      if (Succ == BB) {
        // Ignore myself.
        continue;
      }
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

CachedLoopInfo::CachedLoopInfo(GetLoopInfoT _GetLoopInfo, CareAboutT _CareAbout)
    : GetLoopInfo(std::move(_GetLoopInfo)), CareAbout(std::move(_CareAbout)) {}

CachedLoopInfo::~CachedLoopInfo() {
  // Release the cached static loops.
  std::unordered_set<StaticInnerMostLoop *> Released;
  for (const auto &Iter : this->BBToStaticLoopMap) {
    if (Released.find(Iter.second) == Released.end()) {
      Released.insert(Iter.second);
      DEBUG(llvm::errs() << "Releasing StaticInnerMostLoop at " << Iter.second
                         << '\n');
      delete Iter.second;
    }
  }
  this->BBToStaticLoopMap.clear();
  this->FunctionsWithCachedStaticLoop.clear();
}

StaticInnerMostLoop *CachedLoopInfo::getStaticLoopForBB(llvm::BasicBlock *BB) {
  this->buildStaticLoopsIfNecessary(BB->getParent());
  auto Iter = this->BBToStaticLoopMap.find(BB);
  if (Iter == this->BBToStaticLoopMap.end()) {
    return nullptr;
  }
  return Iter->second;
}

void CachedLoopInfo::buildStaticLoopsIfNecessary(llvm::Function *Function) {
  if (this->FunctionsWithCachedStaticLoop.find(Function) !=
      this->FunctionsWithCachedStaticLoop.end()) {
    // This function has already been processed.
    return;
  }
  auto &LoopInfo = this->GetLoopInfo(*Function);
  for (auto Loop : LoopInfo.getLoopsInPreorder()) {
    if (this->CareAbout(Loop)) {
      // We only cache the statically vectorizable loops.
      auto StaticLoop = new StaticInnerMostLoop(Loop);
      for (auto BB : StaticLoop->BBList) {
        auto Inserted = this->BBToStaticLoopMap.emplace(BB, StaticLoop).second;
        assert(Inserted && "The bb has already been inserted.");
      }
    }
  }
  this->FunctionsWithCachedStaticLoop.insert(Function);
}

#undef DEBUG_TYPE