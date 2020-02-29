#include "TransformUtils.h"

#include "llvm/ADT/SetVector.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

/**
 * I copied this from llvm code base DCE.cpp.
 * The functions there are static and I want to use them.
 */

static bool DCEInstruction(Instruction *I,
                           SmallSetVector<Instruction *, 16> &WorkList,
                           const TargetLibraryInfo *TLI) {
  if (isInstructionTriviallyDead(I, TLI)) {
    // Null out all of the instruction's operands to see if any operand becomes
    // dead as we go.
    for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i) {
      Value *OpV = I->getOperand(i);
      I->setOperand(i, nullptr);

      if (!OpV->use_empty() || I == OpV)
        continue;

      // If the operand is an instruction that became dead as we nulled out the
      // operand, and if it is 'trivially' dead, delete it in a future loop
      // iteration.
      if (Instruction *OpI = dyn_cast<Instruction>(OpV))
        if (isInstructionTriviallyDead(OpI, TLI))
          WorkList.insert(OpI);
    }

    I->eraseFromParent();
    return true;
  }
  return false;
}

/// Call SimplifyCFG on all the blocks in the function,
/// iterating until no more changes are made.
static bool iterativelySimplifyCFG(Function &F,
                                   const TargetTransformInfo &TTI) {
  bool Changed = false;
  bool LocalChange = true;

  while (LocalChange) {
    LocalChange = false;

    // Loop over all of the basic blocks and remove them if they are unneeded.
    for (Function::iterator BBIt = F.begin(); BBIt != F.end();) {
      if (simplifyCFG(&*BBIt++, TTI)) {
        LocalChange = true;
      }
    }
    Changed |= LocalChange;
  }
  return Changed;
}

static bool simplifyFunctionCFG(Function &F, const TargetTransformInfo &TTI) {
  bool EverChanged = removeUnreachableBlocks(F);
  EverChanged |= iterativelySimplifyCFG(F, TTI);

  // If neither pass changed anything, we're done.
  if (!EverChanged)
    return false;

  // iterativelySimplifyCFG can (rarely) make some loops dead.  If this happens,
  // removeUnreachableBlocks is needed to nuke them, which means we should
  // iterate between the two optimizations.  We structure the code like this to
  // avoid rerunning iterativelySimplifyCFG if the second pass of
  // removeUnreachableBlocks doesn't do anything.
  if (!removeUnreachableBlocks(F))
    return true;

  do {
    EverChanged = iterativelySimplifyCFG(F, TTI);
    EverChanged |= removeUnreachableBlocks(F);
  } while (EverChanged);

  return true;
}

bool TransformUtils::eliminateDeadCode(Function &F, TargetLibraryInfo *TLI) {
  bool MadeChange = false;
  SmallSetVector<Instruction *, 16> WorkList;
  // Iterate over the original function, only adding insts to the worklist
  // if they actually need to be revisited. This avoids having to pre-init
  // the worklist with the entire function's worth of instructions.
  for (inst_iterator FI = inst_begin(F), FE = inst_end(F); FI != FE;) {
    Instruction *I = &*FI;
    ++FI;

    // We're visiting this instruction now, so make sure it's not in the
    // worklist from an earlier visit.
    if (!WorkList.count(I))
      MadeChange |= DCEInstruction(I, WorkList, TLI);
  }

  while (!WorkList.empty()) {
    Instruction *I = WorkList.pop_back_val();
    MadeChange |= DCEInstruction(I, WorkList, TLI);
  }
  return MadeChange;
}

bool TransformUtils::simplifyCFG(llvm::Function &F,
                                 llvm::TargetLibraryInfo *TLI,
                                 llvm::TargetTransformInfo &TTI) {
  return simplifyFunctionCFG(F, TTI);
}