
#include "DynamicInstruction.h"
#include "DataGraph.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "DynamicInstruction"

DynamicValue::DynamicValue(const std::string &_Value,
                           const std::string &_MemBase, uint64_t _MemOffset)
    : Value(_Value), MemBase(_MemBase), MemOffset(_MemOffset) {}

DynamicValue::DynamicValue(const DynamicValue &Other)
    : Value(Other.Value), MemBase(Other.MemBase), MemOffset(Other.MemOffset) {}

DynamicValue::DynamicValue(DynamicValue &&Other)
    : Value(std::move(Other.Value)), MemBase(std::move(Other.MemBase)),
      MemOffset(Other.MemOffset) {}

std::string DynamicValue::serializeToBytes(llvm::Type *Type) const {
  std::string Bytes;
  /**
   * I am not sure about &str[0], but there is no other way to directly
   * modify the undeflying array and I do not want to copy again.
   */
  switch (Type->getTypeID()) {
  case llvm::Type::FloatTyID: {
    Bytes.resize(sizeof(float), 0);
    *reinterpret_cast<float *>(&Bytes[0]) = std::stof(this->Value);
    break;
  }
  case llvm::Type::DoubleTyID: {
    Bytes.resize(sizeof(double), 0);
    *reinterpret_cast<double *>(&Bytes[0]) = std::stod(this->Value);
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    assert((BitWidth % 8) == 0 && "Bit width should be multiple of 8.");
    size_t ByteWidth = BitWidth / 8;
    Bytes.resize(ByteWidth);
    /**
     * Notice that when trace, we already ingored the sign so here
     * we just take them as unsigned.
     */
    switch (ByteWidth) {
    case 1: {
      *reinterpret_cast<char *>(&Bytes[0]) = stoul(this->Value);
      break;
    }
    case 4: {
      *reinterpret_cast<uint32_t *>(&Bytes[0]) = stoul(this->Value);
      break;
    }
    case 8: {
      *reinterpret_cast<uint64_t *>(&Bytes[0]) = stoul(this->Value);
      break;
    }
    default: {
      llvm_unreachable("Unsupported integer width.");
      break;
    }
    }
    break;
  }
  case llvm::Type::PointerTyID: {
    // Pointers are treated as uint64_t, base 16.
    Bytes.resize(sizeof(uint64_t), 0);
    *reinterpret_cast<double *>(&Bytes[0]) = std::stoul(this->Value, 0, 16);
    break;
  }
  case llvm::Type::VectorTyID: {
    auto VectorType = llvm::cast<llvm::VectorType>(Type);
    size_t ByteWidth = VectorType->getBitWidth() / 8;
    Bytes.resize(ByteWidth);
    size_t StartPos = 0;
    // Parse the comma separated values (base 16).
    for (size_t Pos = 0; Pos < Bytes.size(); ++Pos) {
      size_t NextCommaPos = this->Value.find(',', StartPos);
      Bytes[Pos] = static_cast<char>(std::stoi(
          this->Value.substr(StartPos, NextCommaPos - StartPos), 0, 16));
      StartPos = NextCommaPos + 1;
    }
    break;
  }
  default: { llvm_unreachable("Unsupported type.\n"); }
  }
  return Bytes;
}

DynamicInstruction::DynamicInstruction()
    : Id(allocateId()), DynamicResult(nullptr) {}

DynamicInstruction::~DynamicInstruction() {
  if (this->DynamicResult != nullptr) {
    delete this->DynamicResult;
    this->DynamicResult = nullptr;
  }
  for (auto &OperandsIter : this->DynamicOperands) {
    if (OperandsIter != nullptr) {
      delete OperandsIter;
      OperandsIter = nullptr;
    }
  }
}

DynamicInstruction::DynamicId DynamicInstruction::InvalidId = 0;

DynamicInstruction::DynamicId DynamicInstruction::allocateId() {
  // Only consider single thread. 0 is reserved.
  static DynamicId CurrentId = 0;
  return ++CurrentId;
}

void DynamicInstruction::format(std::ofstream &Out, DataGraph *Trace) const {
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

void DynamicInstruction::serializeToProtobuf(
    LLVM::TDG::TDGInstruction *ProtobufEntry, DataGraph *DG) const {
  ProtobufEntry->set_op(this->getOpName());
  ProtobufEntry->set_id(this->Id);
  {
    auto DepIter = DG->RegDeps.find(this->Id);
    if (DepIter != DG->RegDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        ProtobufEntry->add_deps(DepInstId);
      }
    }
  }
  {
    auto DepIter = DG->MemDeps.find(this->Id);
    if (DepIter != DG->MemDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        ProtobufEntry->add_deps(DepInstId);
      }
    }
  }
  {
    auto DepIter = DG->CtrDeps.find(this->Id);
    if (DepIter != DG->CtrDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        ProtobufEntry->add_deps(DepInstId);
      }
    }
  }
  this->serializeToProtobufExtra(ProtobufEntry, DG);
}

void DynamicInstruction::formatDeps(std::ofstream &Out,
                                    const DependentMap &RegDeps,
                                    const DependentMap &MemDeps,
                                    const DependentMap &CtrDeps) const {
  // if (this->StaticInstruction != nullptr) {
  //   DEBUG(llvm::errs()
  //               << "Format dependence for "
  //               << this->StaticInstruction->getName() << '\n');
  // }
  {
    auto DepIter = RegDeps.find(this->Id);
    if (DepIter != RegDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = MemDeps.find(this->Id);
    if (DepIter != MemDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = CtrDeps.find(this->Id);
    if (DepIter != CtrDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
}

void DynamicInstruction::formatOpCode(std::ofstream &Out) const {
  Out << this->getOpName();
}

LLVMDynamicInstruction::LLVMDynamicInstruction(
    llvm::Instruction *_StaticInstruction, DynamicValue *_Result,
    std::vector<DynamicValue *> _Operands)
    : DynamicInstruction(), StaticInstruction(_StaticInstruction),
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
    llvm::Instruction *_StaticInstruction, TraceParser::TracedInst &_Parsed)
    : DynamicInstruction(), StaticInstruction(_StaticInstruction),
      OpName(StaticInstruction->getOpcodeName()) {
  if (_Parsed.Result != "") {
    // We do have result.
    this->DynamicResult = new DynamicValue(_Parsed.Result);
  }

  for (const auto &Operand : _Parsed.Operands) {
    this->DynamicOperands.push_back(new DynamicValue(Operand));
  }
}

std::string LLVMDynamicInstruction::getOpName() const {
  if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(this->StaticInstruction)) {
    if (llvm::Function *Callee = CallInst->getCalledFunction()) {
      // Make sure this is not indirect call.
      if (Callee->isIntrinsic()) {
        // If this is a simple memset, translate to a "huge" store instruction.
        auto IntrinsicId = Callee->getIntrinsicID();
        if (IntrinsicId == llvm::Intrinsic::ID::memset) {
          return "memset";
        }
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
  }
  // For other inst, just use the original op code.
  return StaticInstruction->getOpcodeName();
}

void LLVMDynamicInstruction::formatCustomizedFields(std::ofstream &Out,
                                                    DataGraph *Trace) const {
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(this->StaticInstruction)) {
    // base|offset|trace_vaddr|size|
    DynamicValue *LoadedAddr = this->DynamicOperands[0];
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
    DynamicValue *StoredAddr = this->DynamicOperands[1];
    llvm::Type *StoredType = StoreStaticInstruction->getPointerOperandType()
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

  if (auto CallStaticInstruction =
          llvm::dyn_cast<llvm::CallInst>(this->StaticInstruction)) {
    if (auto Callee = CallStaticInstruction->getCalledFunction()) {
      // Make sure this is no indirect call.
      if (Callee->isIntrinsic()) {
        auto IntrinsicId = Callee->getIntrinsicID();
        if (IntrinsicId == llvm::Intrinsic::ID::memset) {
          // base|offset|trace_vaddr|size|value|

          /**
           * Note that for intrinsic function the first operand is
           * the first argument.
           * memset(addr, val, size)
           * Also, make this a giantic vector store.
           */
          DynamicValue *StoredAddr = this->DynamicOperands[0];
          uint64_t StoredSize = std::stoul(this->DynamicOperands[2]->Value);
          Out << StoredAddr->MemBase << '|' << StoredAddr->MemOffset << '|'
              << StoredAddr->Value << '|' << StoredSize << '|'
              << this->DynamicOperands[1]->Value << '|';
          return;
        }
      }
    }
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

void LLVMDynamicInstruction::serializeToProtobufExtra(
    LLVM::TDG::TDGInstruction *ProtobufEntry, DataGraph *DG) const {
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(this->StaticInstruction)) {
    // addr|size|base|offset|[new_base]
    DynamicValue *LoadedAddr = this->DynamicOperands[0];
    uint64_t LoadedSize = DG->DataLayout->getTypeStoreSize(
        LoadStaticInstruction->getPointerOperandType()
            ->getPointerElementType());
    auto LoadExtra = ProtobufEntry->mutable_load();
    LoadExtra->set_addr(std::stoul(LoadedAddr->Value));
    LoadExtra->set_size(LoadedSize);
    LoadExtra->set_base(LoadedAddr->MemBase);
    LoadExtra->set_offset(LoadedAddr->MemOffset);
    if (this->DynamicResult->MemBase != "") {
      // This load inst will produce some new base for future memory access.
      DEBUG(llvm::errs() << "Set new base for load "
                         << this->DynamicResult->MemBase << '\n');
      LoadExtra->set_new_base(this->DynamicResult->MemBase);
    }
    return;
  }

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(this->StaticInstruction)) {
    // addr|size|value|base|offset
    DynamicValue *StoredAddr = this->DynamicOperands[1];
    llvm::Type *StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredSize = DG->DataLayout->getTypeStoreSize(StoredType);
    auto StoreExtra = ProtobufEntry->mutable_store();
    StoreExtra->set_addr(std::stoul(StoredAddr->Value));
    StoreExtra->set_size(StoredSize);
    StoreExtra->set_value(
        this->DynamicOperands[0]->serializeToBytes(StoredType));
    StoreExtra->set_base(StoredAddr->MemBase);
    StoreExtra->set_offset(StoredAddr->MemOffset);

    if (StoreExtra->size() != StoreExtra->value().size()) {
      DEBUG(llvm::errs() << "size " << StoreExtra->size() << " value size "
                         << StoreExtra->value().size() << '\n');
      DEBUG(llvm::errs() << "Stored type: ");
      DEBUG(StoredType->print(llvm::errs()));
      DEBUG(llvm::errs() << "\n");
    }

    assert(StoreExtra->size() == StoreExtra->value().size() &&
           "Unmatched size and value size for store.");

    return;
  }

  if (auto CallStaticInstruction =
          llvm::dyn_cast<llvm::CallInst>(this->StaticInstruction)) {
    if (auto Callee = CallStaticInstruction->getCalledFunction()) {
      // Make sure this is no indirect call.
      if (Callee && Callee->isIntrinsic()) {
        auto IntrinsicId = Callee->getIntrinsicID();
        if (IntrinsicId == llvm::Intrinsic::ID::memset) {
          /**
           * Special case for memset, which is translated to a
           * store. Store value will be automatically
           * replicated to the store size by the simulator later.
           *
           * addr|size|value|base|offset
           *
           * Note that for intrinsic function the first dynamic operand is
           * the first argument.
           * memset(addr, value, size)
           */
          DynamicValue *StoredAddr = this->DynamicOperands[0];
          uint64_t StoredSize = std::stoul(this->DynamicOperands[2]->Value);
          auto StoreExtra = ProtobufEntry->mutable_store();
          StoreExtra->set_addr(std::stoul(StoredAddr->Value));
          StoreExtra->set_size(StoredSize);
          StoreExtra->set_value(this->DynamicOperands[1]->serializeToBytes(
              CallStaticInstruction->getFunctionType()->getParamType(1)));
          StoreExtra->set_base(StoredAddr->MemBase);
          StoreExtra->set_offset(StoredAddr->MemOffset);

          return;
        }
      }
    }
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(this->StaticInstruction)) {
    // addr|size|new_base
    uint64_t AllocatedSize = DG->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    auto AllocExtra = ProtobufEntry->mutable_alloc();
    AllocExtra->set_addr(std::stoul(this->DynamicResult->Value));
    AllocExtra->set_size(AllocatedSize);
    AllocExtra->set_new_base(this->DynamicResult->MemBase);
    // This load inst will produce some new base for future memory access.
    DEBUG(llvm::errs() << "Set new base for alloc "
                       << AllocExtra->new_base() << '\n');
    return;
  }

  // For conditional branching, we also log the target basic block.
  bool IsConditionalBranch = false;
  if (auto BranchStaticInstruction =
          llvm::dyn_cast<llvm::BranchInst>(this->StaticInstruction)) {
    IsConditionalBranch = BranchStaticInstruction->isConditional();
  } else if (llvm::isa<llvm::SwitchInst>(this->StaticInstruction)) {
    IsConditionalBranch = true;
  }

  if (IsConditionalBranch) {
    /**
     * Get the next basic block by looking ahead.
     * This will require that there is at least one more instruction
     * in the buffer. Otherwise, we just panic.
     */
    auto NextIter = DG->getDynamicInstFromId(this->Id);
    NextIter++;
    while (NextIter != DG->DynamicInstructionList.end()) {
      // Check if this is an LLVM inst, i.e. not our inserted.
      if ((*NextIter)->getStaticInstruction() != nullptr) {
        break;
      }
      NextIter++;
    }
    assert(NextIter != DG->DynamicInstructionList.end() &&
           "Failed getting next basic block for conditional branch.");
    std::string NextBBName =
        (*NextIter)->getStaticInstruction()->getParent()->getName();
    auto BranchExtra = ProtobufEntry->mutable_branch();
    BranchExtra->set_static_id(
        reinterpret_cast<uint64_t>(this->StaticInstruction));
    BranchExtra->set_next_bb(NextBBName);
    return;
  }
}

#undef DEBUG_TYPE