#include "stream/ae/StreamDataGraph.h"
#include "stream/StaticStream.h"

#include "LoopUtils.h"
#include "Utils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"

#include <list>
#include <vector>

StreamDataGraph::StreamDataGraph(const llvm::Loop *_Loop,
                                 const llvm::Value *_AddrValue,
                                 IsIndOrCmpVarFuncT IsIndOrCmpVar)
    : ExecutionDataGraph(_AddrValue), Loop(_Loop),
      HasPHINodeInComputeInsts(false), HasCallInstInComputeInsts(false) {
  this->constructDataGraph(IsIndOrCmpVar);
  this->HasCircle = this->detectCircle();

  for (const auto &ComputeInst : this->ComputeInsts) {
    if (llvm::isa<llvm::PHINode>(ComputeInst)) {
      this->HasPHINodeInComputeInsts = true;
    }
    if (Utils::isCallOrInvokeInst(ComputeInst)) {
      // Unless it's some supported intrinsics.
      if (auto Callee = Utils::getCalledFunction(ComputeInst)) {
        if (Callee->isIntrinsic()) {
          auto IntrinsicId = Callee->getIntrinsicID();
          if (IntrinsicId == llvm::Intrinsic::maxnum) {
            continue;
          } else if (IntrinsicId ==
                     llvm::Intrinsic::experimental_vector_reduce_v2_fadd) {
            continue;
          }
        }
      }

      this->HasCallInstInComputeInsts = true;
    }
  }
}

void StreamDataGraph::constructDataGraph(IsIndOrCmpVarFuncT IsIndOrCmpVar) {
  std::list<const llvm::Value *> Queue;
  Queue.emplace_back(this->getSingleResultValue());

  std::unordered_set<const llvm::Value *> UnsortedInputs;

  while (!Queue.empty()) {
    auto Value = Queue.front();
    Queue.pop_front();
    auto Inst = llvm::dyn_cast<llvm::Instruction>(Value);
    if (Inst == nullptr) {
      // This is not an instruction, should be an input value unless it's an
      // constant data or an intrinsic.
      if (auto ConstantData = llvm::dyn_cast<llvm::ConstantData>(Value)) {
        this->ConstantValues.insert(ConstantData);
        continue;
      }
      if (auto ConstantVec = llvm::dyn_cast<llvm::ConstantVector>(Value)) {
        this->ConstantValues.insert(ConstantVec);
        continue;
      }
      if (auto Func = llvm::dyn_cast<llvm::Function>(Value)) {
        continue;
      }
      UnsortedInputs.insert(Value);
      continue;
    }
    if (!this->Loop->contains(Inst)) {
      // This is an instruction falling out of the loop, should also be an input
      // value.
      UnsortedInputs.insert(Inst);
      continue;
    }

    if (IsIndOrCmpVar(Inst)) {
      // This is an induction variable stream, should also be an input value.
      UnsortedInputs.insert(Inst);
      continue;
    }

    if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
      // This is a load stream, should also be an input value.
      UnsortedInputs.insert(LoadInst);
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
   * Generate the sorted inputs list, (actually just get a fixed order).
   */
  this->Inputs.insert(this->Inputs.end(), UnsortedInputs.begin(),
                      UnsortedInputs.end());
}

void StreamDataGraph::format(std::ostream &OStream) const {
  OStream << "StreamDataGraph of Loop " << LoopUtils::getLoopId(this->Loop)
          << " Value " << Utils::formatLLVMValue(this->getSingleResultValue())
          << '\n';
  OStream << "-------------- Input Values ---------------\n";
  for (const auto &Input : this->Inputs) {
    OStream << Utils::formatLLVMValue(Input) << '\n';
  }
  OStream << "-------------- Compute Insts --------------\n";
  if (auto AddrInst =
          llvm::dyn_cast<llvm::Instruction>(this->getSingleResultValue())) {
    if (this->ComputeInsts.count(AddrInst) == 0) {
      // This address instruction is somehow loop invariant.
      assert(this->ComputeInsts.empty() &&
             "If the address instruction is not in the loop, there should be "
             "no compute instructions.");
    } else {
      std::unordered_set<const llvm::Instruction *> FormattedInsts;
      auto FormatTask =
          [&OStream, &FormattedInsts](const llvm::Instruction *Inst) -> void {
        if (FormattedInsts.count(Inst) == 0) {
          OStream << Utils::formatLLVMInst(Inst);
          for (unsigned OperandIdx = 0, NumOperands = Inst->getNumOperands();
               OperandIdx != NumOperands; ++OperandIdx) {
            OStream << ' '
                    << Utils::formatLLVMValue(Inst->getOperand(OperandIdx));
          }
          OStream << '\n';
          FormattedInsts.insert(Inst);
        }
      };
      if (this->HasCircle) {
        OStream << "!!! Circle !!!\n";
        for (const auto &ComputeInst : this->ComputeInsts) {
          FormatTask(ComputeInst);
        }
      } else {
        this->dfsOnComputeInsts(AddrInst, FormatTask);
      }
    }
  } else {
    assert(this->ComputeInsts.empty() &&
           "If there is no address instruction, there should be no compute "
           "instructions.");
  }
  OStream << "-------------- StreamDataGraph End -------\n";
}

/**
 * Two address datagraphs can be coalesce iff.
 * 1.
 */
bool StreamDataGraph::isAbleToCoalesceWith(const StreamDataGraph &Other) const {
  return false;
}

std::pair<StreamDataGraph::ValueList, StreamDataGraph::InstSet>
StreamDataGraph::sliceWithFusedOp(const StreamSet &Streams) const {

  // Remove fused LoadOps.
  InstSet NewComputeInsts;
  for (const auto &Inst : this->ComputeInsts) {
    bool Fused = false;
    for (auto S : Streams) {
      for (auto FusedLoadOp : S->FusedLoadOps) {
        if (FusedLoadOp == Inst) {
          Fused = true;
          break;
        }
      }
      if (Fused) {
        break;
      }
    }
    if (!Fused) {
      llvm::errs() << "Push in ComputeInst " << Utils::formatLLVMValue(Inst)
                   << '\n';
      NewComputeInsts.insert(Inst);
    }
  }

  // Replace the inputs and remove unused ones.
  ValueList NewInputs;
  for (const auto &Input : this->Inputs) {
    auto FinalInputValue = Input;
    if (auto InputInst = llvm::dyn_cast<llvm::Instruction>(Input)) {
      for (auto S : Streams) {
        if (S->Inst != InputInst) {
          continue;
        }
        FinalInputValue = S->getFinalFusedLoadInst();
      }
    }
    bool StillUsed = false;
    for (const auto &ComputeInst : NewComputeInsts) {
      for (auto OperandIdx = 0; OperandIdx < ComputeInst->getNumOperands();
           ++OperandIdx) {
        auto Operand = ComputeInst->getOperand(OperandIdx);
        if (Operand == FinalInputValue) {
          StillUsed = true;
          break;
        }
      }
      if (StillUsed) {
        break;
      }
    }
    /**
     * Special case for empty function, e.g. memcpy, memmove, where the input
     * is used as the final value.
     */
    for (auto ResultValue : this->ResultValues) {
      if (FinalInputValue == ResultValue) {
        StillUsed = true;
      }
    }
    if (StillUsed) {
      llvm::errs() << "Push in Input "
                   << Utils::formatLLVMValue(FinalInputValue) << '\n';
      NewInputs.push_back(FinalInputValue);
    }
  }

  return std::make_pair(NewInputs, NewComputeInsts);
}

llvm::Function *StreamDataGraph::generateFunctionWithFusedOp(
    const std::string &FuncName, std::unique_ptr<llvm::Module> &Module,
    const StreamSet &Streams, bool IsLoad) {

  /**
   * ! This is very hacky:
   * We replace the Input and remove the FusedLoadOps from my ComputeInsts.
   * TODO: Handle this more elegantly in the future.
   */
  auto OriginalInputs = this->Inputs;
  auto OriginalComputeInsts = this->ComputeInsts;

  auto Ret = this->sliceWithFusedOp(Streams);
  this->Inputs = Ret.first;
  this->ComputeInsts = Ret.second;

  auto Func = this->generateFunction(FuncName, Module, IsLoad);

  this->Inputs = OriginalInputs;
  this->ComputeInsts = OriginalComputeInsts;
  return Func;
}

StreamDataGraph::ValueList
StreamDataGraph::getInputsWithFusedOp(const StreamSet &Streams) {
  return this->sliceWithFusedOp(Streams).first;
}
