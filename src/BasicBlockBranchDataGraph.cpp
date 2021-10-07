#include "BasicBlockBranchDataGraph.h"

#include "Utils.h"

#define DEBUG_TYPE "BasicBlockBranchDataGraph"

BBBranchDataGraph::BBBranchDataGraph(const llvm::Loop *_Loop,
                                     const llvm::BasicBlock *_BB,
                                     const std::string _Suffix)
    : ExecutionDataGraph(), Loop(_Loop), BB(_BB),
      FuncName(llvm::Twine(_BB->getParent()->getName() + "_" +
                           _Loop->getHeader()->getName() + "_" +
                           _BB->getName() + _Suffix)
                   .str()) {
  this->constructDataGraph();
}

void BBBranchDataGraph::constructDataGraph() {
  auto Terminator = this->BB->getTerminator();
  auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(Terminator);
  if (!BranchInst) {
    // Can not analyze non-branch instruction.
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Non-Branch "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return;
  }
  if (!BranchInst->isConditional()) {
    // Not a conditional branch.
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Unconditional "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return;
  }
  if (BranchInst->getNumSuccessors() != 2) {
    // Too many targets.
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Too Many Targets "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return;
  }
  auto TrueBB = BranchInst->getSuccessor(0);
  auto FalseBB = BranchInst->getSuccessor(1);
  // Start bfs on the conditional value to construct the datagraph.
  std::list<const llvm::Value *> Queue;
  Queue.emplace_back(BranchInst->getCondition());
  std::unordered_set<const llvm::Value *> UnsortedInputs;
  while (!Queue.empty()) {
    auto Value = Queue.front();
    Queue.pop_front();
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Value);
    if (!Inst) {
      // This is not an instruction, should be an input value unless it's an
      // constant data.
      if (auto ConstantData = llvm::dyn_cast<llvm::ConstantData>(Value)) {
        this->ConstantValues.insert(ConstantData);
      } else if (auto ConstantVec =
                     llvm::dyn_cast<llvm::ConstantVector>(Value)) {
        this->ConstantValues.insert(ConstantVec);
      } else {
        UnsortedInputs.insert(Value);
      }
      continue;
    }
    if (!this->Loop->contains(Inst)) {
      // This is an instruction falling out of the loop, should also be an input
      // value.
      UnsortedInputs.insert(Inst);
      continue;
    }
    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
      // We simply handle phi node as inputs.
      UnsortedInputs.insert(PHINode);
      this->InputPHIs.insert(PHINode);
      continue;
    }
    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
      // This is a load stream, should also be an input value.
      UnsortedInputs.insert(LoadInst);
      this->InputLoads.insert(LoadInst);
      continue;
    }
    // This is an compute instruction.
    bool Inserted = this->ComputeInsts.insert(Inst).second;
    if (Inserted) {
      // The instruction is inserted, which means that it is an new instruction.
      for (unsigned OperandIdx = 0, NumOperand = Inst->getNumOperands();
           OperandIdx != NumOperand; ++OperandIdx) {
        Queue.emplace_back(Inst->getOperand(OperandIdx));
      }
    }
  }
  /**
   * Ensure that:
   * 1. All InputLoads are from the same BB.
   * 2. No InputPHIs.
   */
  bool IsValidTemp = true;
  for (auto InputLoad : this->InputLoads) {
    if (InputLoad->getParent() != this->BB) {
      LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Other BB Load "
                              << Utils::formatLLVMBB(this->BB) << '\n');
      IsValidTemp = false;
    }
  }
  if (!this->InputPHIs.empty()) {
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: PHI Input "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    IsValidTemp = false;
  }
  if (IsValidTemp) {
    // Setup.
    this->IsValid = true;
    this->ResultValues.emplace_back(BranchInst->getCondition());
    LLVM_DEBUG(llvm::dbgs()
               << "BBPredDG Valid: " << Utils::formatLLVMBB(this->BB) << '\n');
    /**
     * Generate the sorted inputs list, (actually just
     * get a fixed order).
     */
    this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                        UnsortedInputs.end());
    this->TrueBB = TrueBB;
    this->FalseBB = FalseBB;
    this->HasCircle = this->detectCircle();
  } else {
    // Clean up.
    this->clear();
  }
}

void BBBranchDataGraph::clear() {
  this->IsValid = false;
  this->InputPHIs.clear();
  this->InputLoads.clear();
  this->ComputeInsts.clear();
  this->TrueBB = nullptr;
  this->FalseBB = nullptr;
}

bool BBBranchDataGraph::isPredicate(const llvm::BasicBlock *TargetBB) const {
  assert((TargetBB == this->TrueBB || TargetBB == this->FalseBB) &&
         "Invalid Predicate TargetBB");
  if (!this->isValid() || !TargetBB) {
    return false;
  }
  if (TargetBB == this->BB) {
    // Do not allow self predicate.
    return false;
  }
  return TargetBB->getSinglePredecessor() == this->BB;
}

bool BBBranchDataGraph::isLoopHeadPredicate(
    const llvm::BasicBlock *TargetBB) const {
  assert((TargetBB == this->TrueBB || TargetBB == this->FalseBB) &&
         "Invalid LoopHead TargetBB");
  if (!this->isValid() || !TargetBB) {
    return false;
  }
  if (TargetBB == this->BB) {
    // Do not allow self predicate.
    return false;
  }
  llvm::Loop *TargetLoop = nullptr;
  for (auto SubLoop : this->Loop->getSubLoops()) {
    if (SubLoop->getHeader() == TargetBB) {
      TargetLoop = SubLoop;
      break;
    }
  }
  if (!TargetLoop) {
    // TargetBB is not a LoopHeader.
    return false;
  }
  for (auto Predecessor : llvm::predecessors(TargetBB)) {
    if (TargetLoop->contains(Predecessor)) {
      continue;
    }
    if (Predecessor != this->BB) {
      // TargetBB has other predecessors from loop outside, return false.
      return false;
    }
  }
  return true;
}

bool BBBranchDataGraph::isValidLoopBoundPredicate() const {
  if (!this->isValid()) {
    return false;
  }
  if (this->Loop->getExitingBlock() != this->BB) {
    LLVM_DEBUG(llvm::dbgs() << "LoopBoundPredDG Invalid: Not Single ExitingBB "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return false;
  }
  if (this->Loop->getLoopLatch() != this->BB) {
    LLVM_DEBUG(llvm::dbgs() << "LoopBoundPredDG Invalid: Not Single Latch "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return false;
  }
  if (this->Loop->getNumBlocks() != 1) {
    // This is very restricted.
    LLVM_DEBUG(llvm::dbgs() << "LoopBoundPredDG Invalid: Not Single BB "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return false;
  }
  return true;
}

CachedBBBranchDataGraph::~CachedBBBranchDataGraph() {
  for (auto &Entry : this->KeyToDGMap) {
    delete Entry.second;
  }
  this->KeyToDGMap.clear();
}

BBBranchDataGraph *
CachedBBBranchDataGraph::getBBBranchDataGraph(const llvm::Loop *Loop,
                                              const llvm::BasicBlock *BB) {
  auto K = std::make_pair(Loop, BB);
  auto Iter = this->KeyToDGMap.find(K);
  if (Iter == this->KeyToDGMap.end()) {
    auto BBPredDG = new BBBranchDataGraph(Loop, BB);
    this->KeyToDGMap.emplace(K, BBPredDG);
    return BBPredDG;
  }
  return Iter->second;
}

BBBranchDataGraph *
CachedBBBranchDataGraph::tryBBBranchDataGraph(const llvm::Loop *Loop,
                                              const llvm::BasicBlock *BB) {
  auto K = std::make_pair(Loop, BB);
  auto Iter = this->KeyToDGMap.find(K);
  if (Iter == this->KeyToDGMap.end()) {
    return nullptr;
  }
  return Iter->second;
}