
#include "DynamicInstruction.h"
#include "DataGraph.h"

#include "llvm/Support/raw_ostream.h"

DynamicValue::DynamicValue(const std::string& _Value,
                           const std::string& _MemBase, uint64_t _MemOffset)
    : Value(_Value), MemBase(_MemBase), MemOffset(_MemOffset) {}

DynamicValue::DynamicValue(const DynamicValue& Other)
    : Value(Other.Value), MemBase(Other.MemBase), MemOffset(Other.MemOffset) {}

DynamicInstruction::DynamicInstruction()
    : Id(allocateId()), DynamicResult(nullptr) {}

DynamicInstruction::~DynamicInstruction() {
  if (this->DynamicResult != nullptr) {
    delete this->DynamicResult;
    this->DynamicResult = nullptr;
  }
  for (auto& OperandsIter : this->DynamicOperands) {
    if (OperandsIter != nullptr) {
      delete OperandsIter;
      OperandsIter = nullptr;
    }
  }
}

DynamicInstruction::DynamicId DynamicInstruction::allocateId() {
  // Only consider single thread.
  static DynamicId CurrentId = 0;
  return CurrentId++;
}

void DynamicInstruction::format(std::ofstream& Out, DataGraph* Trace) const {
  this->formatOpCode(Out);
  Out << '|';
  // The faked number of micro ops.
  uint32_t FakeNumMicroOps = 1;
  Out << this->Id << '|';
  // The dependence field.
  this->formatDeps(Out, Trace->RegDeps, Trace->MemDeps, Trace->CtrDeps);
  Out << '|';
  // Other fields for other insts.
  this->formatCustomizedFields(Out, Trace);
}

void DynamicInstruction::formatDeps(std::ofstream& Out,
                                    const DependentMap& RegDeps,
                                    const DependentMap& MemDeps,
                                    const DependentMap& CtrDeps) const {
  // if (this->StaticInstruction != nullptr) {
  //   DEBUG(llvm::errs()
  //               << "Format dependence for "
  //               << this->StaticInstruction->getName() << '\n');
  // }
  {
    auto DepIter = RegDeps.find(this->Id);
    if (DepIter != RegDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = MemDeps.find(this->Id);
    if (DepIter != MemDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = CtrDeps.find(this->Id);
    if (DepIter != CtrDeps.end()) {
      for (const auto& DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
}

void DynamicInstruction::formatOpCode(std::ofstream& Out) const {
  Out << this->getOpName();
}

LLVMDynamicInstruction::LLVMDynamicInstruction(
    llvm::Instruction* _StaticInstruction, DynamicValue* _Result,
    std::vector<DynamicValue*> _Operands)
    : DynamicInstruction(),
      StaticInstruction(_StaticInstruction),
      OpName(StaticInstruction->getOpcodeName()) {
  this->DynamicResult = _Result;
  this->DynamicOperands = std::move(_Operands);

  assert(this->StaticInstruction != nullptr &&
         "Non null static instruction ptr.");
  if (this->DynamicResult != nullptr) {
    // Sanity check to make sure that the static instruction do has result.
    assert(this->StaticInstruction->getName() != "" &&
           "DynamicResult set for non-result static instruction.");
  } else {
    // Comment this out as for some call inst, if the callee is traced,
    // then we donot log the result.
    if (!llvm::isa<llvm::CallInst>(_StaticInstruction)) {
      assert(this->StaticInstruction->getName() == "" &&
             "Missing DynamicResult for non-call instruction.");
    }
  }
}

LLVMDynamicInstruction::LLVMDynamicInstruction(
    llvm::Instruction* _StaticInstruction, TraceParser::TracedInst& _Parsed)
    : DynamicInstruction(),
      StaticInstruction(_StaticInstruction),
      OpName(StaticInstruction->getOpcodeName()) {
  if (_Parsed.Result != "") {
    // We do have result.
    this->DynamicResult = new DynamicValue(_Parsed.Result);
  }

  for (const auto& Operand : _Parsed.Operands) {
    this->DynamicOperands.push_back(new DynamicValue(Operand));
  }
}

std::string LLVMDynamicInstruction::getOpName() const {
  if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(this->StaticInstruction)) {
    llvm::Function* Callee = CallInst->getCalledFunction();
    if (Callee->isIntrinsic()) {
      // For instrinsic, use "call_intrinsic" for now.
      return "call_intrinsic";
    } else if (Callee->getName() == "sin") {
      return "sin";
    } else if (Callee->getName() == "cos") {
      return "cos";
    } else {
      return "call";
    }
  }
  // For other inst, just use the original op code.
  return StaticInstruction->getOpcodeName();
}

void LLVMDynamicInstruction::formatCustomizedFields(std::ofstream& Out,
                                                    DataGraph* Trace) const {
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(this->StaticInstruction)) {
    // base|offset|trace_vaddr|size|
    DynamicValue* LoadedAddr = this->DynamicOperands[0];
    uint64_t LoadedSize = Trace->DataLayout->getTypeStoreSize(
        LoadStaticInstruction->getPointerOperandType()
            ->getPointerElementType());
    Out << LoadedAddr->MemBase << '|' << LoadedAddr->MemOffset << '|'
        << LoadedAddr->Value << '|' << LoadedSize << '|';
    // This load inst will produce some new base for future memory access.
    Out << this->DynamicResult->MemBase << '|';
    return;
  }

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(this->StaticInstruction)) {
    // base|offset|trace_vaddr|size|type_id|type_name|value|
    DynamicValue* StoredAddr = this->DynamicOperands[1];
    llvm::Type* StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredSize = Trace->DataLayout->getTypeStoreSize(StoredType);
    std::string TypeName;
    {
      llvm::raw_string_ostream Stream(TypeName);
      StoredType->print(Stream);
    }
    Out << StoredAddr->MemBase << '|' << StoredAddr->MemOffset << '|'
        << StoredAddr->Value << '|' << StoredSize << '|'
        << StoredType->getTypeID() << '|' << TypeName << '|'
        << this->DynamicOperands[0]->Value << '|';
    return;
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(this->StaticInstruction)) {
    // base|trace_vaddr|size|
    uint64_t AllocatedSize = Trace->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    Out << this->DynamicResult->MemBase << '|' << this->DynamicResult->Value
        << '|' << AllocatedSize << '|';
    return;
  }

  // For conditional branching, we also log the target basic block.
  if (auto BranchStaticInstruction =
          llvm::dyn_cast<llvm::BranchInst>(this->StaticInstruction)) {
    if (BranchStaticInstruction->isConditional()) {
      auto NextIter = Trace->getDynamicInstFromId(this->Id);
      NextIter++;
      while (NextIter != Trace->DynamicInstructionList.end()) {
        // Check if this is an LLVM inst, i.e. not our inserted.
        if ((*NextIter)->getStaticInstruction() != nullptr) {
          break;
        }
        NextIter++;
      }
      // Log the static instruction's address and target branch name.
      // Use the memory address as the identifier for static instruction
      // is very hacky, but it does maintain unique.
      std::string NextBBName =
          (*NextIter)->getStaticInstruction()->getParent()->getName();
      Out << BranchStaticInstruction << '|' << NextBBName << '|';
    }
    return;
  }

  // For switch, do the same.
  // TODO: Reuse code with branch case.
  if (auto SwitchStaticInstruction =
          llvm::dyn_cast<llvm::SwitchInst>(this->StaticInstruction)) {
    auto NextIter = Trace->getDynamicInstFromId(this->Id);
    NextIter++;
    while (NextIter != Trace->DynamicInstructionList.end()) {
      // Check if this is an LLVM inst, i.e. not our inserted.
      if ((*NextIter)->getStaticInstruction() != nullptr) {
        break;
      }
      NextIter++;
    }
    // Log the static instruction's address and target branch name.
    // Use the memory address as the identifier for static instruction
    // is very hacky, but it does maintain unique.
    std::string NextBBName =
        (*NextIter)->getStaticInstruction()->getParent()->getName();
    Out << SwitchStaticInstruction << '|' << NextBBName << '|';
    return;
  }
}