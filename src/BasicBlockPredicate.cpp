#include "BasicBlockPredicate.h"

#include "Utils.h"

#define DEBUG_TYPE "BasicBlockPredicate"

BBPredicateDataGraph::BBPredicateDataGraph(const llvm::Loop *_Loop,
                                           const llvm::BasicBlock *_BB)
    : ExecutionDataGraph(nullptr), Loop(_Loop), BB(_BB),
      FuncName(llvm::Twine(_BB->getParent()->getName() + "_" +
                           _Loop->getHeader()->getName() + "_" +
                           _BB->getName() + "_pred")
                   .str()) {
  this->constructDataGraph();
}

void BBPredicateDataGraph::constructDataGraph() {
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
  if (TrueBB == this->BB || FalseBB == this->BB) {
    // Do not allow branching back to myself.
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Branch Back "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return;
  }
  if (!(this->Loop->contains(TrueBB) && this->Loop->contains(FalseBB))) {
    // Both BB should be within our loop.
    LLVM_DEBUG(llvm::dbgs() << "BBPredDG Invalid: Outside Successor "
                            << Utils::formatLLVMBB(this->BB) << '\n');
    return;
  }
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
        this->ConstantDatas.insert(ConstantData);
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
    this->ResultValue = BranchInst->getCondition();
    LLVM_DEBUG(llvm::dbgs()
               << "BBPredDG Valid: " << Utils::formatLLVMBB(this->BB) << '\n');
    /**
     * Generate the sorted inputs list, (actually just
     * get a fixed order).
     */
    this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                        UnsortedInputs.end());
    // Check if we predicate the True/False BB.
    if (TrueBB->getSinglePredecessor() == this->BB) {
      LLVM_DEBUG(llvm::dbgs()
                 << "BBPredDG Add TrueBB: " << Utils::formatLLVMBB(TrueBB)
                 << " to " << Utils::formatLLVMBB(this->BB) << '\n');
      this->TrueBB = TrueBB;
    }
    if (FalseBB->getSinglePredecessor() == this->BB) {
      LLVM_DEBUG(llvm::dbgs()
                 << "BBPredDG Add FalseBB: " << Utils::formatLLVMBB(FalseBB)
                 << " to " << Utils::formatLLVMBB(this->BB) << '\n');
      this->FalseBB = FalseBB;
    }
    this->HasCircle = this->detectCircle();
  } else {
    // Clean up.
    this->IsValid = false;
    this->InputPHIs.clear();
    this->InputLoads.clear();
    this->ComputeInsts.clear();
  }
}

CachedBBPredicateDataGraph::~CachedBBPredicateDataGraph() {
  for (auto &Entry : this->KeyToDGMap) {
    delete Entry.second;
  }
  this->KeyToDGMap.clear();
}

BBPredicateDataGraph *CachedBBPredicateDataGraph::getBBPredicateDataGraph(
    const llvm::Loop *Loop, const llvm::BasicBlock *BB) {
  auto K = std::make_pair(Loop, BB);
  auto Iter = this->KeyToDGMap.find(K);
  if (Iter == this->KeyToDGMap.end()) {
    auto BBPredDG = new BBPredicateDataGraph(Loop, BB);
    this->KeyToDGMap.emplace(K, BBPredDG);
    return BBPredDG;
  }
  return Iter->second;
}

BBPredicateDataGraph *CachedBBPredicateDataGraph::tryBBPredicateDataGraph(
    const llvm::Loop *Loop, const llvm::BasicBlock *BB) {
  auto K = std::make_pair(Loop, BB);
  auto Iter = this->KeyToDGMap.find(K);
  if (Iter == this->KeyToDGMap.end()) {
    return nullptr;
  }
  return Iter->second;
}