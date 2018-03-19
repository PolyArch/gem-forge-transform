#include "IdenticalTransformer.h"

#include "llvm/Support/raw_ostream.h"

#include <fstream>

IdenticalTransformer::IdenticalTransformer(DynamicTrace* _Trace,
                                           const std::string& _OutTraceName)
    : Trace(_Trace) {
  // Simply generate the output data graph for gem5 to use.
  std::ofstream OutTrace(_OutTraceName);
  assert(OutTrace.is_open() && "Failed to open output trace file.");

  for (DynamicId Id = 0, NumDynamicInsts = this->Trace->DyanmicInstsMap.size();
       Id != NumDynamicInsts; ++Id) {
    DynamicInstruction* DynamicInst = this->Trace->DyanmicInstsMap.at(Id);
    this->formatInstruction(DynamicInst, OutTrace);
    OutTrace << '\n';
  }

  OutTrace.close();
}

void IdenticalTransformer::formatOpCode(llvm::Instruction* StaticInstruction,
                                        std::ofstream& Out) {
  if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(StaticInstruction)) {
    llvm::Function* Callee = CallInst->getCalledFunction();
    if (Callee->isIntrinsic()) {
      // For instrinsic, use "call_intrinsic" for now.
      Out << "call_intrinsic";
    } else if (Callee->getName() == "sin") {
      Out << "sin";
    } else if (Callee->getName() == "cos") {
      Out << "cos";
    } else {
      Out << "call";
    }
    return;
  }
  // For other inst, just use the original op code.
  Out << StaticInstruction->getOpcodeName();
}

void IdenticalTransformer::formatDeps(DynamicInstruction* DynamicInst,
                                      std::ofstream& Out) {
  {
    auto DepIter = this->Trace->RegDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->RegDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->MemDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->MemDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
  {
    auto DepIter = this->Trace->MemDeps.find(DynamicInst->Id);
    if (DepIter != this->Trace->MemDeps.end()) {
      for (const auto& DepId : DepIter->second) {
        Out << DepId << ',';
      }
    }
  }
}

void IdenticalTransformer::formatInstruction(DynamicInstruction* DynamicInst,
                                             std::ofstream& Out) {
  // The op_code field.
  this->formatOpCode(DynamicInst->StaticInstruction, Out);
  Out << '|';
  // The dependence field.
  this->formatDeps(DynamicInst, Out);
  Out << '|';
  // Other fields for other isnsts.
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->StaticInstruction)) {
    // base|offset|trace_vaddr|size|
    DynamicValue* LoadedAddr = DynamicInst->DynamicOperands[0];
    uint64_t LoadedSize = this->Trace->DataLayout->getTypeStoreSize(
        LoadStaticInstruction->getPointerOperandType()->getPointerElementType());
    Out << LoadedAddr->MemBase << '|' << LoadedAddr->MemOffset << '|'
        << LoadedAddr->Value << '|' << LoadedSize << '|';
    return;
  }

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(DynamicInst->StaticInstruction)) {
    // base|offset|trace_vaddr|size|type_id|type_name|value|
    DynamicValue* StoredAddr = DynamicInst->DynamicOperands[1];
    llvm::Type* StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredSize = this->Trace->DataLayout->getTypeStoreSize(StoredType);
    std::string TypeName;
    {
      llvm::raw_string_ostream Stream(TypeName);
      StoredType->print(Stream);
    }
    Out << StoredAddr->MemBase << '|' << StoredAddr->MemOffset << '|'
        << StoredAddr->Value << '|' << StoredSize << '|'
        << StoredType->getTypeID() << '|' << TypeName << '|'
        << DynamicInst->DynamicOperands[0]->Value << '|';
    return;
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(DynamicInst->StaticInstruction)) {
    // base|trace_vaddr|size|
    uint64_t AllocatedSize = this->Trace->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    Out << DynamicInst->DynamicResult->MemBase << '|'
        << DynamicInst->DynamicResult->Value << '|' << AllocatedSize << '|';
    return;
  }
}
