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
