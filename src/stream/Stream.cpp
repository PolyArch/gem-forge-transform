#include "stream/Stream.h"
#include "Utils.h"

void Stream::searchAddressComputeInstructions() {
  std::list<ContextInst> Queue;

  llvm::Value *AddrValue = nullptr;
  if (llvm::isa<llvm::LoadInst>(this->CInst.Inst)) {
    AddrValue = this->CInst.Inst->getOperand(0);
  } else {
    AddrValue = this->CInst.Inst->getOperand(1);
  }

  if (auto AddrInst = llvm::dyn_cast<llvm::Instruction>(AddrValue)) {
    Queue.emplace_back(this->CInst.Context, AddrInst);
  }

  while (!Queue.empty()) {
    auto CurrentInst = Queue.front();
    Queue.pop_front();
    if (this->AddrInsts.count(CurrentInst) != 0) {
      // We have already processed this one.
      continue;
    }
    if (!this->CLoop.contains(CurrentInst)) {
      // This instruction is out of our analysis level. ignore it.
      continue;
    }
    if (Utils::isCallOrInvokeInst(CurrentInst.Inst)) {
      // So far I do not know how to process the call/invoke instruction.
      continue;
    }

    this->AddrInsts.insert(CurrentInst);
    if (llvm::isa<llvm::LoadInst>(CurrentInst.Inst)) {
      // This is also a base load.
      this->BaseLoads.insert(CurrentInst);
      // Do not go further for load.
      continue;
    }

    // BFS on the operands.
    for (unsigned OperandIdx = 0,
                  NumOperands = CurrentInst.Inst->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = CurrentInst.Inst->getOperand(OperandIdx);
      if (auto OperandInst = llvm::dyn_cast<llvm::Instruction>(OperandValue)) {
        // This is an instruction within the same context.
        Queue.emplace_back(CurrentInst.Context, OperandInst);
      } else if (auto OperandArg =
                     llvm::dyn_cast<llvm::Argument>(OperandValue)) {
        // This is an argument. Try to search upwards in the context.
        InlineContextPtr NewContext = CurrentInst.Context;
        auto ArgInst = Stream::resolveArgument(NewContext, OperandArg);
        if (ArgInst != nullptr) {
          // We successfully resolve the argument.
          Queue.emplace_back(NewContext, ArgInst);
        }
      }
    }
  }
}

llvm::Instruction *Stream::resolveArgument(InlineContextPtr &Context,
                                           llvm::Argument *Arg) {
  while (!Context->empty()) {
    auto ArgNo = Arg->getArgNo();
    auto CallSiteInst = Context->Context.back();
    auto ArgOperandValue = Utils::getArgOperand(CallSiteInst, ArgNo);
    if (auto ArgOperandInst =
            llvm::dyn_cast<llvm::Instruction>(ArgOperandValue)) {
      Context = Context->pop();
      return ArgOperandInst;
    } else if (auto ArgOperandArg =
                   llvm::dyn_cast<llvm::Argument>(ArgOperandValue)) {
      // This is another operand. Keep search upwards.
      Context = Context->pop();
      Arg = ArgOperandArg;
    } else {
      // Some how this is not an instruction. (May be constant).
      break;
    }
  }
  return nullptr;
}