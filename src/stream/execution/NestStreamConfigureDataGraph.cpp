#include "NestStreamConfigureDataGraph.h"

#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "NestStreamConfigureDataGraph"

NestStreamConfigureDataGraph::NestStreamConfigureDataGraph(
    const llvm::Loop *_OuterLoop, const llvm::BasicBlock *_ConfigBB)
    : ExecutionDataGraph(), OuterLoop(_OuterLoop), ConfigBB(_ConfigBB),
      FuncName(llvm::Twine(_ConfigBB->getParent()->getName() + "_" +
                           _OuterLoop->getHeader()->getName() + "_" +
                           _ConfigBB->getName() + "_nest")
                   .str()) {
  this->constructDataGraph();
}

void NestStreamConfigureDataGraph::constructDataGraph() {
  LLVM_DEBUG(llvm::dbgs() << "[Nest] Construct for " << this->FuncName
                          << ".\n";);
  std::list<const llvm::Value *> Queue;
  for (auto InstIter = this->ConfigBB->begin(), InstEnd = this->ConfigBB->end();
       InstIter != InstEnd; ++InstIter) {
    auto Inst = &*InstIter;
    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      auto Callee = CallInst->getCalledFunction();
      if (!Callee || !Callee->isIntrinsic()) {
        continue;
      }
      auto IntrinsicID = Callee->getIntrinsicID();
      switch (IntrinsicID) {
      default:
        break;
      case llvm::Intrinsic::ssp_stream_config:
      case llvm::Intrinsic::ssp_stream_input:
      case llvm::Intrinsic::ssp_stream_ready: {
        this->ResultValues.emplace_back(Inst);
        this->ComputeInsts.insert(Inst);
        for (unsigned Idx = 0; Idx < CallInst->getNumArgOperands(); ++Idx) {
          auto Arg = CallInst->getArgOperand(Idx);
          Queue.emplace_back(Arg);
        }
        break;
      }
      }
    }
  }
  // Start bfs on the StreamInput value to construct the datagraph.
  std::unordered_set<const llvm::Value *> UnsortedInputs;
  while (!Queue.empty()) {
    auto Value = Queue.front();
    Queue.pop_front();
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Value);
    if (!Inst) {
      // This is not an instruction, should be an input value unless it's
      // an constant data.
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
    if (!this->OuterLoop->contains(Inst)) {
      // This is an instruction falling out of the loop, should also be an
      // input value.
      UnsortedInputs.insert(Inst);
      continue;
    }
    if (auto PHINode = llvm::dyn_cast<llvm::PHINode>(Inst)) {
      // We simply handle phi node as inputs.
      UnsortedInputs.insert(PHINode);
      continue;
    }
    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
      // This is a load stream, should also be an input value.
      UnsortedInputs.insert(LoadInst);
      continue;
    }
    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
      UnsortedInputs.insert(CallInst);
      continue;
    }
    // This is an compute instruction.
    bool Inserted = this->ComputeInsts.insert(Inst).second;
    if (Inserted) {
      // The instruction is inserted, which means that it is an new
      // instruction.
      for (unsigned OperandIdx = 0, NumOperand = Inst->getNumOperands();
           OperandIdx != NumOperand; ++OperandIdx) {
        Queue.emplace_back(Inst->getOperand(OperandIdx));
      }
    }
  }
  /**
   * Ensure that:
   * 1. All Inputs are from StreamLoad.
   * 2. Or are from a PHINode.
   * 2. Or are just values outside of OuterLoop.
   */
  bool IsValidTemp = true;
  for (auto Input : UnsortedInputs) {
    LLVM_DEBUG({
      llvm::dbgs() << "[Nest] Checking Input ";
      Input->print(llvm::dbgs());
      llvm::dbgs() << '\n';
    });
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Input);
    if (!Inst) {
      continue;
    }
    if (!this->OuterLoop->contains(Inst)) {
      continue;
    }
    if (llvm::isa<llvm::PHINode>(Inst)) {
      continue;
    }
    auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst);
    if (!CallInst) {
      IsValidTemp = false;
      break;
    }
    auto Callee = CallInst->getCalledFunction();
    if (!Callee || !Callee->isIntrinsic()) {
      IsValidTemp = false;
      break;
    }
    auto IntrinsicID = Callee->getIntrinsicID();
    if (IntrinsicID != llvm::Intrinsic::ssp_stream_load) {
      IsValidTemp = false;
      break;
    }
  }
  if (IsValidTemp) {
    // Setup.
    this->IsValid = true;
    /**
     * Generate the sorted inputs list, (actually just
     * get a fixed order).
     */
    this->hoistInputToCompute(UnsortedInputs);
    this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                        UnsortedInputs.end());
    this->HasCircle = this->detectCircle();
  } else {
    // Clean up.
    this->IsValid = false;
    this->ComputeInsts.clear();
    this->ResultValues.clear();
  }
}

void NestStreamConfigureDataGraph::hoistInputToCompute(
    std::unordered_set<const llvm::Value *> &UnsortedInputs) {

  while (true) {

    std::unordered_set<const llvm::Instruction *> PendingRemoveInputs;

    for (auto Input : UnsortedInputs) {
      auto Inst = llvm::dyn_cast<llvm::Instruction>(Input);
      if (!Inst || OuterLoop->contains(Inst)) {
        // This is not an instruction or not from outside of OuterLoop.
        continue;
      }
      /**
       * For now we only apply this optimization to simple arithmetic inst.
       */

      LLVM_DEBUG({
        llvm::dbgs() << "[Nest] Check CanHoist ";
        Inst->print(llvm::dbgs());
        llvm::dbgs() << '\n';
      });
      auto InstOp = Inst->getOpcode();
      if (InstOp != llvm::Instruction::Add &&
          InstOp != llvm::Instruction::Shl) {
        LLVM_DEBUG(llvm::dbgs() << "[Nest] NotHoist InvalidOp.\n");
        continue;
      }
      // Check that all operand are either constant or already another input.
      bool CanHoist = true;
      for (auto Operand : Inst->operand_values()) {
        if (UnsortedInputs.count(Operand)) {
          continue;
        }
        // This is not an input, should be an input value unless it's
        // an constant data.
        if (auto ConstantData = llvm::dyn_cast<llvm::ConstantData>(Operand)) {
          continue;
        } else if (auto ConstantVec =
                       llvm::dyn_cast<llvm::ConstantVector>(Operand)) {
          continue;
        } else {
          LLVM_DEBUG(llvm::dbgs() << "[Nest] NotHoist NotConstant Operand.\n");
          CanHoist = false;
          break;
        }
      }

      if (!CanHoist) {
        continue;
      }

      // Remove the input from UnsortedInputs.
      LLVM_DEBUG(llvm::dbgs() << "[Nest] CanHoist.\n");
      PendingRemoveInputs.insert(Inst);
    }

    if (PendingRemoveInputs.empty()) {
      break;
    }
    for (auto Inst : PendingRemoveInputs) {
      // Try to remove the input.
      for (auto Operand : Inst->operand_values()) {
        // This is not an instruction, should be an input value unless it's
        // an constant data.
        if (auto ConstantData = llvm::dyn_cast<llvm::ConstantData>(Operand)) {
          this->ConstantValues.insert(ConstantData);
        } else if (auto ConstantVec =
                       llvm::dyn_cast<llvm::ConstantVector>(Operand)) {
          this->ConstantValues.insert(ConstantVec);
        }
      }

      // Add the input as one of the compute.
      this->ComputeInsts.insert(Inst);
      UnsortedInputs.erase(Inst);
    }
  }
}