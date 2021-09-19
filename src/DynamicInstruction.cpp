
#include "DynamicInstruction.h"
#include "DataGraph.h"
#include "Utils.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "DynamicInstruction"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

DynamicValue::DynamicValue(const std::string &_Value,
                           const std::string &_MemBase, uint64_t _MemOffset)
    : Value(_Value), MemBase(_MemBase), MemOffset(_MemOffset) {}

DynamicValue::DynamicValue(const DynamicValue &Other)
    : Value(Other.Value), MemBase(Other.MemBase), MemOffset(Other.MemOffset) {}

DynamicValue &DynamicValue::operator=(const DynamicValue &Other) {
  if (this == &Other) {
    return *this;
  }
  this->Value = Other.Value;
  this->MemBase = Other.MemBase;
  this->MemOffset = Other.MemOffset;
  return *this;
}

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
    *reinterpret_cast<float *>(&Bytes[0]) = this->getFloat();
    break;
  }
  case llvm::Type::DoubleTyID: {
    Bytes.resize(sizeof(double), 0);
    *reinterpret_cast<double *>(&Bytes[0]) = this->getDouble();
    break;
  }
  case llvm::Type::IntegerTyID: {
    auto IntegerType = llvm::cast<llvm::IntegerType>(Type);
    unsigned BitWidth = IntegerType->getBitWidth();
    if ((BitWidth % 8) != 0) {
      LLVM_DEBUG(llvm::dbgs() << "Bit width of integer " << BitWidth << '\n');
    }
    if (BitWidth == 1) {
      // Sometimes, llvm optimize boolean to i1, but this is at least 1 bytes.
      BitWidth = 8;
    }
    assert((BitWidth % 8) == 0 && "Bit width should be multiple of 8.");
    size_t ByteWidth = BitWidth / 8;
    Bytes.resize(ByteWidth);
    /**
     * Notice that when trace, we already ingored the sign so here
     * we just take them as unsigned.
     */
    switch (ByteWidth) {
    case 1: {
      *reinterpret_cast<char *>(&Bytes[0]) = this->getInt();
      break;
    }
    case 2: {
      *reinterpret_cast<uint16_t *>(&Bytes[0]) = this->getInt();
      break;
    }
    case 4: {
      *reinterpret_cast<uint32_t *>(&Bytes[0]) = this->getInt();
      break;
    }
    case 8: {
      *reinterpret_cast<uint64_t *>(&Bytes[0]) = this->getInt();
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
    *reinterpret_cast<uint64_t *>(&Bytes[0]) = this->getAddr();
    break;
  }
  case llvm::Type::FixedVectorTyID: {
    Bytes = this->Value;
    // auto VectorType = llvm::cast<llvm::VectorType>(Type);
    // size_t ByteWidth = VectorType->getBitWidth() / 8;
    // Bytes.resize(ByteWidth);
    // size_t StartPos = 0;
    // // Parse the comma separated values (base 16).
    // for (size_t Pos = 0; Pos < Bytes.size(); ++Pos) {
    //   size_t NextCommaPos = this->Value.find(',', StartPos);
    //   Bytes[Pos] = static_cast<char>(std::stoi(
    //       this->Value.substr(StartPos, NextCommaPos - StartPos), 0, 16));
    //   StartPos = NextCommaPos + 1;
    // }
    break;
  }
  default: {
    llvm_unreachable("Unsupported type.\n");
  }
  }
  return Bytes;
}

DynamicInstruction::DynamicInstruction()
    : DynamicResult(nullptr), Id(allocateId()), UsedStreamIds(nullptr) {}

DynamicInstruction::DynamicInstruction(DynamicId _Id)
    : DynamicResult(nullptr), Id(_Id), UsedStreamIds(nullptr) {}

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
  if (this->UsedStreamIds != nullptr) {
    delete this->UsedStreamIds;
    this->UsedStreamIds = nullptr;
  }
}

DynamicInstruction::DynamicId DynamicInstruction::InvalidId = 0;

DynamicInstruction::DynamicId DynamicInstruction::allocateId() {
  // Only consider single thread. 0 is reserved.
  static DynamicId CurrentId = 0;
  return ++CurrentId;
}

void DynamicInstruction::addUsedStreamId(uint64_t UsedStreamId) {
  if (this->UsedStreamIds == nullptr) {
    this->UsedStreamIds = new std::unordered_set<uint64_t>();
  }
  this->UsedStreamIds->insert(UsedStreamId);
}

void DynamicInstruction::format(llvm::raw_ostream &Out, DataGraph *DG) const {
  this->formatOpCode(Out);
  Out << '|';
  // The faked number of micro ops.
  uint32_t FakeNumMicroOps = 1;
  Out << this->getId() << '|';
  // The dependence field.
  this->formatDeps(Out, DG);
  Out << '|';
  // Other fields for other insts.
  this->formatCustomizedFields(Out, DG);
  Out << '\n';
}

void DynamicInstruction::serializeToProtobuf(
    LLVM::TDG::TDGInstruction *ProtobufEntry, DataGraph *DG) const {
  ProtobufEntry->set_op(this->getOpName());
  ProtobufEntry->set_id(this->getId());

  /**
   * We set the bb field if we are the first non-phi instruction in the block.
   * We can set bb field for all instruction, but we do this as a trivial
   * optimization on the size of the data graph.
   */
  auto StaticInst = this->getStaticInstruction();
  if (StaticInst != nullptr) {
    ProtobufEntry->set_pc(
        reinterpret_cast<uint64_t>(Utils::getInstUIDMap().getUID(StaticInst)));
    auto BB = StaticInst->getParent();
    if (BB->getFirstNonPHI() == StaticInst) {
      /**
       * We use the BB's address directly as the block id.
       * This statisfy two principles:
       * 1. Global uniqueness: no two bb have the same id.
       * 2. 0 is reserved for invalid bb. (no NULL).
       */
      ProtobufEntry->set_bb(reinterpret_cast<uint64_t>(BB));
    }
  }

  {
    /**
     * A simple deduplication on the register deps.
     */
    std::unordered_set<DynamicId> RegDepsSeen;
    auto DepIter = DG->RegDeps.find(this->getId());
    if (DepIter != DG->RegDeps.end()) {
      for (const auto &RegDep : DepIter->second) {
        if (RegDepsSeen.find(RegDep.second) == RegDepsSeen.end()) {
          auto Dep = ProtobufEntry->add_deps();
          Dep->set_type(::LLVM::TDG::TDGInstructionDependence::REGISTER);
          Dep->set_dependent_id(RegDep.second);
          RegDepsSeen.insert(RegDep.second);
        }
      }
    }
  }
  {
    auto DepIter = DG->MemDeps.find(this->getId());
    if (DepIter != DG->MemDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        auto Dep = ProtobufEntry->add_deps();
        Dep->set_type(::LLVM::TDG::TDGInstructionDependence::MEMORY);
        Dep->set_dependent_id(DepInstId);
      }
    }
  }
  {
    auto DepIter = DG->CtrDeps.find(this->getId());
    if (DepIter != DG->CtrDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        auto Dep = ProtobufEntry->add_deps();
        Dep->set_type(::LLVM::TDG::TDGInstructionDependence::CONTROL);
        Dep->set_dependent_id(DepInstId);
      }
    }
  }

  {
    if (this->UsedStreamIds != nullptr) {
      for (const auto &UsedStreamId : (*this->UsedStreamIds)) {
        auto Dep = ProtobufEntry->add_deps();
        Dep->set_type(::LLVM::TDG::TDGInstructionDependence::STREAM);
        Dep->set_dependent_id(UsedStreamId);
      }
    }
  }

  // For other dependence information within the instruction.
  for (const auto &Dep : this->Deps) {
    auto ProtobufDep = ProtobufEntry->add_deps();
    ProtobufDep->set_type(Dep.Type);
    ProtobufDep->set_dependent_id(Dep.Id);
  }

  this->serializeToProtobufExtra(ProtobufEntry, DG);
}

void DynamicInstruction::formatDeps(llvm::raw_ostream &Out,
                                    DataGraph *DG) const {
  {
    auto DepIter = DG->RegDeps.find(this->getId());
    if (DepIter != DG->RegDeps.end()) {
      for (const auto &RegDep : DepIter->second) {
        Out << RegDep.second << ',';
      }
    }
  }
  {
    auto DepIter = DG->MemDeps.find(this->getId());
    if (DepIter != DG->MemDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
  {
    auto DepIter = DG->CtrDeps.find(this->getId());
    if (DepIter != DG->CtrDeps.end()) {
      for (const auto &DepInstId : DepIter->second) {
        Out << DepInstId << ',';
      }
    }
  }
}

void DynamicInstruction::formatOpCode(llvm::raw_ostream &Out) const {
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
        if (IntrinsicId == llvm::Intrinsic::memset) {
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

void LLVMDynamicInstruction::formatCustomizedFields(llvm::raw_ostream &Out,
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
        if (IntrinsicId == llvm::Intrinsic::memset) {
          // base|offset|trace_vaddr|size|value|

          /**
           * Note that for intrinsic function the first operand is
           * the first argument.
           * memset(addr, val, size)
           * Also, make this a giantic vector store.
           */
          DynamicValue *StoredAddr = this->DynamicOperands[0];
          uint64_t StoredSize = this->DynamicOperands[2]->getInt();
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
      auto NextIter = Trace->getDynamicInstFromId(this->getId());
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
      auto NextBBName =
          (*NextIter)->getStaticInstruction()->getParent()->getName();
      Out << BranchStaticInstruction << '|' << NextBBName << '|';
    }
    return;
  }

  // For switch, do the same.
  // TODO: Reuse code with branch case.
  if (auto SwitchStaticInstruction =
          llvm::dyn_cast<llvm::SwitchInst>(this->StaticInstruction)) {
    auto NextIter = Trace->getDynamicInstFromId(this->getId());
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
    auto NextBBName =
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
    LoadExtra->set_addr(LoadedAddr->getAddr());
    LoadExtra->set_size(LoadedSize);
    LoadExtra->set_base(LoadedAddr->MemBase);
    LoadExtra->set_offset(LoadedAddr->MemOffset);
    if (this->DynamicResult->MemBase != "") {
      // This load inst will produce some new base for future memory access.
      LLVM_DEBUG(llvm::dbgs() << "Set new base for load "
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
    StoreExtra->set_addr(StoredAddr->getAddr());
    StoreExtra->set_size(StoredSize);
    StoreExtra->set_value(
        this->DynamicOperands[0]->serializeToBytes(StoredType));
    StoreExtra->set_base(StoredAddr->MemBase);
    StoreExtra->set_offset(StoredAddr->MemOffset);

    if (StoreExtra->size() != StoreExtra->value().size()) {
      LLVM_DEBUG(llvm::dbgs() << "size " << StoreExtra->size() << " value size "
                              << StoreExtra->value().size() << '\n');
      LLVM_DEBUG(llvm::dbgs() << "Stored type: ");
      LLVM_DEBUG(StoredType->print(llvm::errs()));
      LLVM_DEBUG(llvm::dbgs() << "\n");
    }

    assert(StoreExtra->size() == StoreExtra->value().size() &&
           "Unmatched size and value size for store.");

    return;
  }

  if (auto CallStaticInstruction =
          llvm::dyn_cast<llvm::CallInst>(this->StaticInstruction)) {
    if (auto Callee = CallStaticInstruction->getCalledFunction()) {
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
      if (Callee && Callee->isIntrinsic()) {
        auto IntrinsicId = Callee->getIntrinsicID();
        if (IntrinsicId == llvm::Intrinsic::memset) {
          DynamicValue *StoredAddr = this->DynamicOperands[0];
          uint64_t StoredSize = this->DynamicOperands[2]->getInt();
          auto StoreExtra = ProtobufEntry->mutable_store();
          StoreExtra->set_addr(StoredAddr->getAddr());
          StoreExtra->set_size(StoredSize);
          StoreExtra->set_value(this->DynamicOperands[1]->serializeToBytes(
              CallStaticInstruction->getFunctionType()->getParamType(1)));
          StoreExtra->set_base(StoredAddr->MemBase);
          StoreExtra->set_offset(StoredAddr->MemOffset);

          return;
        }
      }
    }
    // No return here as CallInst may also be branch inst.
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(this->StaticInstruction)) {
    // addr|size|new_base
    uint64_t AllocatedSize = DG->DataLayout->getTypeStoreSize(
        AllocaStaticInstruction->getAllocatedType());
    auto AllocExtra = ProtobufEntry->mutable_alloc();
    AllocExtra->set_addr(this->DynamicResult->getAddr());
    AllocExtra->set_size(AllocatedSize);
    AllocExtra->set_new_base(this->DynamicResult->MemBase);
    // This alloc inst will produce some new base for future memory access.
    // LLVM_DEBUG(llvm::dbgs() << "Set new base for alloc " <<
    // AllocExtra->new_base()
    //                    << '\n');
    return;
  }

  if (Utils::isAsmBranchInst(this->StaticInstruction)) {
    /**
     * For branching instructions, we use the first successor block as the
     * static next pc, and the the real target in the dynamic next pc.
     */

    /**
     * Get the dynamic next pc.
     * This will require that there is at least one more instruction
     * in the buffer. Otherwise, we just return.
     */

    auto DynamicNextStaticInst = this->getDynamicNextStaticInst(DG);
    if (DynamicNextStaticInst == nullptr) {
      static size_t MissingDynamicNextStaticInsts = 0;
      MissingDynamicNextStaticInsts++;
      llvm::errs() << "Failed getting next basic block for "
                      "branch, MISSING "
                   << MissingDynamicNextStaticInsts << '\n';
      return;
    }
    auto DynamicNextPC = Utils::getInstUIDMap().getUID(DynamicNextStaticInst);

    /**
     * Get the static next pc.
     * If failed, i.e. this is the last static inst in the function.
     * We use 0.
     */
    auto StaticNextInst =
        Utils::getStaticNextNonPHIInst(this->StaticInstruction);
    auto StaticNextPC = (StaticNextInst != nullptr)
                            ? Utils::getInstUIDMap().getUID(StaticNextInst)
                            : 0;

    auto BranchExtra = ProtobufEntry->mutable_branch();
    BranchExtra->set_static_next_pc(StaticNextPC);
    BranchExtra->set_dynamic_next_pc(DynamicNextPC);
    BranchExtra->set_is_conditional(
        Utils::isAsmConditionalBranchInst(this->StaticInstruction));
    BranchExtra->set_is_indirect(
        Utils::isAsmIndirectBranchInst(this->StaticInstruction));
    return;
  }
}

const llvm::Instruction *
LLVMDynamicInstruction::getDynamicNextStaticInst(DataGraph *DG) const {

  if (auto BranchInst =
          llvm::dyn_cast<llvm::BranchInst>(this->StaticInstruction)) {
    // For branch instruction, we determine the next basic block from
    // the dynamic value. We do not use the dynamic instruction from
    // the datagraph as some transformation may remove dynamic instructions from
    // the DG, which result's in wrong results.

    // Determine the dynamic target BB.
    const llvm::BasicBlock *DynamicNextBB = nullptr;
    if (BranchInst->isConditional()) {
      // Conditional branch.
      assert(this->DynamicOperands.size() > 0 &&
             "Missing dynamic branching operand.");
      auto DynamicBranchOperand = this->DynamicOperands.at(0)->getInt();
      if (DynamicBranchOperand == 1) {
        // Branch takes true.
        DynamicNextBB = BranchInst->getSuccessor(0);
      } else {
        // Branch takes false.
        DynamicNextBB = BranchInst->getSuccessor(1);
      }
    } else {
      // Unconditional branch.
      DynamicNextBB = BranchInst->getSuccessor(0);
    }

    // Return the first non-phi instruction.
    return DynamicNextBB->getFirstNonPHI();
  }

  if (auto SwitchInst =
          llvm::dyn_cast<llvm::SwitchInst>(this->StaticInstruction)) {
    // Similar for switch inst.
    const llvm::BasicBlock *DynamicNextBB = SwitchInst->getDefaultDest();
    // Check for cases.
    assert(this->DynamicOperands.size() > 0 &&
           "Missing dynamic switch operand.");
    auto DynamicSwitchOperand = this->DynamicOperands.at(0)->getInt();
    for (auto CaseIter = SwitchInst->case_begin(),
              CaseEnd = SwitchInst->case_end();
         CaseIter != CaseEnd; ++CaseIter) {
      auto CaseValue = CaseIter->getCaseValue();
      if (CaseValue->equalsInt(DynamicSwitchOperand)) {
        // Found the matching case.
        DynamicNextBB = CaseIter->getCaseSuccessor();
        break;
      }
    }
    return DynamicNextBB->getFirstNonPHI();
  }

  auto NextIter = DG->getDynamicInstFromId(this->getId());
  NextIter++;
  while (NextIter != DG->DynamicInstructionList.end()) {
    // Check if this is an LLVM inst, i.e. not our inserted ones.
    if ((*NextIter)->getStaticInstruction() != nullptr) {
      break;
    }
    NextIter++;
  }
  if (NextIter == DG->DynamicInstructionList.end()) {
    return nullptr;
  }
  assert(NextIter != DG->DynamicInstructionList.end() &&
         "Failed getting next basic block for conditional branch.");
  // To avoid the case some instructions in the DG are removed,
  // we do not return the static instruction directly, but the
  // first non-phi instruction from the BB as a remedy.
  auto NextStaticInst = (*NextIter)->getStaticInstruction();
  return NextStaticInst->getParent()->getFirstNonPHI();
}

#undef DEBUG_TYPE