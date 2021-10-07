#ifndef LLVM_TDG_LOOP_UNROLLER_H
#define LLVM_TDG_LOOP_UNROLLER_H

#include "DataGraph.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

/**
 * This class helps to unroll a loop.
 * It maintains some internal data structures to relax the register and control
 * dependence to effectively unroll the loop. However, it will not insert/delete
 * any instructions, which means the induction variable are still updated
 * multiple times.
 *
 * It will simply unroll what ever the user provides. It is the user's
 * responsibility to ensure that there is full iterations to be unrolled.
 *
 */
class LoopUnroller {
public:
  LoopUnroller(llvm::Loop *_Loop, llvm::ScalarEvolution *SE);

  /**
   * If there is no induction variable found, we think this loop can not be
   * unrolled.
   */
  bool canUnroll() const {
    if (!this->Loop->empty()) {
      return false;
    }
    return !this->InductionVars.empty();
  }

  /**
   * Unroll the buffered instructions.
   *
   * We introduce some concepts on dependence. An dependence is
   * 1. Outside, if the dependent inst is outside our unroll window.
   * 2. Inside, if the dependent inst is within our unroll window.
   *    2.1. Inter-iter, if the dependent inst is from previous iteration.
   *    2.2. Intra-iter, if the dependent inst is from the same iteration.
   *
   * If the loop can not be unrolled, we simply return.
   * Otherwise, we mainly relaxes the dependence as:
   * 1. Relax the register dependence on induction variables to the start value.
   * Notice that some times the start value does not come from an instruction,
   * in such case we simply remove the register dependence.
   * 2. For inter-iter control dependence, we remove the inter-iter control
   * dependence, and insert all the outside control dependence to the inst as
   * serialization between different unrolled iteration.
   */
  void unroll(DataGraph *DG, DataGraph::DynamicInstIter Begin,
              DataGraph::DynamicInstIter End);

  /**
   * Hacky way to provide a utility function to check for induction variable.
   */
  bool isInductionVariable(llvm::Instruction *Inst) const;
  bool isReductionVariable(llvm::Instruction *Inst) const;

  llvm::Instruction *getUnrollableTerminator() const {
    return this->UnrollableTerminator;
  }

private:
  using DynamicId = DynamicInstruction::DynamicId;

  llvm::Loop *Loop;

  // Map from the induction variable to its descriptor.
  std::unordered_map<llvm::Instruction *, llvm::InductionDescriptor>
      InductionVars;

  std::unordered_map<llvm::Instruction *, llvm::RecurrenceDescriptor>
      ReductionVars;

  llvm::Instruction *UnrollableTerminator;

  /**
   * Updating fields when unrolling.
   */

  // The current iteration.
  int CurrentIter;

  /**
   * A map from induction variable to the id of the dynamic instruction which
   * produces its start value. If the start value does not come from an
   * instruction, it stores InvalidId.
   */
  std::unordered_map<llvm::Instruction *, DynamicId> IVToStartDynamicIdMap;

  /**
   * Maps the dynamic id to its iteration in the stream.
   * This is used to detect inter-iteration dependence.
   */
  std::unordered_map<DynamicId, int> DynamicIdToIterationMap;

  /**
   * Record all the outside control dependence. This is used for serialization.
   */
  std::set<DynamicId> OutsideCtrDeps;

  /**
   * Update the IVToStartDynamicIdMap. If this is not the first iteration, we
   * simply ignore it.
   */
  void updateIVToStartDynamicIdMap(llvm::Instruction *IV, DynamicId Id);

  /**
   * Update the current iteration count.
   */
  void updateCurrentIter(DynamicInstruction *DynamicInst);

  /**
   * Fix the register dependence of an induction variable. It also call
   * updateIVToStartDynamicIdMap to update.
   *
   * Change induction variable dependence to the start value.
   */
  void fixAndUpdateIVRegDeps(DynamicInstruction *DynamicInst, DataGraph *DG);

  /**
   * Ignore the intra-iter control dependence.
   * Remove the inter-iter control dependence, and insert outside control
   * dependence.
   */
  void fixAndUpdateCtrDeps(DynamicInstruction *DynamicInst, DataGraph *DG);

  /**
   * Find the update binary operation of induction variable.
   */
  // llvm::BinaryOperator *findUpdatingBinaryOp(llvm::PHINode *IV) const;
};

class CachedLoopUnroller {
public:
  CachedLoopUnroller() = default;
  ~CachedLoopUnroller();

  LoopUnroller *getUnroller(llvm::Loop *Loop, llvm::ScalarEvolution *SE);

private:
  std::unordered_map<llvm::Loop *, LoopUnroller *> CachedLU;
};

#endif