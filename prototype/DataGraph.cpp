#include "DataGraph.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

#define DEBUG_TYPE "DataGraph"

DynamicValue::DynamicValue(const std::string& _Value)
    : Value(_Value), MemBase(""), MemOffset(0) {}

DynamicInstruction::DynamicInstruction()
    : DynamicResult(nullptr), Prev(nullptr), Next(nullptr) {}

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

LLVMDynamicInstruction::LLVMDynamicInstruction(
    llvm::Instruction* _StaticInstruction, DynamicValue* _Result,
    std::vector<DynamicValue*> _Operands, DynamicInstruction* _Prev,
    DynamicInstruction* _Next)
    : DynamicInstruction(),
      StaticInstruction(_StaticInstruction),
      OpName(StaticInstruction->getOpcodeName()) {
  this->DynamicResult = _Result;
  this->DynamicOperands = std::move(_Operands);

  // Set the _Prev, _Next.
  this->Prev = _Prev;
  this->Next = _Next;

  assert(this->StaticInstruction != nullptr &&
         "Non null static instruction ptr.");
  if (this->DynamicResult != nullptr) {
    // Sanity check to make sure that the static instruction do has result.
    if (this->StaticInstruction->getName() == "") {
      DEBUG(
          llvm::errs() << "DynamicResult set for non-result static instruction "
                       << this->StaticInstruction->getName() << '\n');
    }
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
    llvm::Instruction* _StaticInstruction, TraceParser::TracedInst& _Parsed,
    DynamicInstruction* _Prev, DynamicInstruction* _Next)
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

  // Set the _Prev, _Next.
  this->Prev = _Prev;
  this->Next = _Next;

  assert(this->StaticInstruction != nullptr &&
         "Non null static instruction ptr.");
  if (this->DynamicResult != nullptr) {
    // Sanity check to make sure that the static instruction do has result.
    if (this->StaticInstruction->getName() == "") {
      DEBUG(
          llvm::errs() << "DynamicResult set for non-result static instruction "
                       << this->StaticInstruction->getName() << '\n');
    }
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

DataGraph::DataGraph(TraceParser* _Parser, llvm::Module* _Module)
    : DynamicInstructionListHead(nullptr),
      DynamicInstructionListTail(nullptr),
      Module(_Module),
      NumMemDependences(0),
      Parser(_Parser),
      CurrentFunctionName(""),
      CurrentBasicBlockName(""),
      CurrentIndex(-1) {
  this->DataLayout = new llvm::DataLayout(this->Module);

  while (true) {
    auto NextType = this->Parser->getNextType();
    if (NextType == TraceParser::END) {
      break;
    }
    switch (NextType) {
      case TraceParser::INST: {
        auto Parsed = this->Parser->parseLLVMInstruction();
        this->parseDynamicInstruction(Parsed);
        break;
      }
      case TraceParser::FUNC_ENTER: {
        auto Parsed = this->Parser->parseFunctionEnter();
        this->parseFunctionEnter(Parsed);
        break;
      }
      default: {
        llvm_unreachable("Unknown next type.");
        break;
      }
    }
  }
}

DataGraph::~DataGraph() {
  DynamicInstruction* Iter = this->DynamicInstructionListHead;
  while (Iter != nullptr) {
    auto Prev = Iter;
    Iter = Iter->Next;
    delete Prev;
  }
  this->DynamicInstructionListHead = nullptr;
  this->DynamicInstructionListTail = nullptr;
  delete this->DataLayout;
  delete this->Parser;
}

void DataGraph::parseDynamicInstruction(TraceParser::TracedInst& Parsed) {
  llvm::Instruction* StaticInstruction =
      this->getLLVMInstruction(Parsed.Func, Parsed.BB, Parsed.Id);
  if (Parsed.Op != StaticInstruction->getOpcodeName()) {
    DEBUG(llvm::errs() << "Unmatched static opcode "
                       << StaticInstruction->getOpcodeName()
                       << " dynamic opcode " << Parsed.Op);
  }
  assert(Parsed.Op == StaticInstruction->getOpcodeName() &&
         "Unmatched opcode.");

  // Check the operands.
  assert(Parsed.Operands.size() == StaticInstruction->getNumOperands() &&
         "Unmatched # of dynamic value for operand.");

  // Special case: Result of ret instruction.
  // This can only happen when we returned from a traced
  // callee, the extra result belongs to the "call" instruction
  // in caller, e.g.
  // caller::call
  // caller::call::param
  // ...
  // callee::ret
  // callee::ret::param
  // caller::call::result
  // For now simply ignore it.
  if (llvm::isa<llvm::ReturnInst>(StaticInstruction) && Parsed.Result != "") {
    // Free the dynamic result for now.
    Parsed.Result = "";
  }

  // Create the new dynamic instructions.
  DynamicInstruction* DynamicInst = new LLVMDynamicInstruction(
      StaticInstruction, Parsed, this->DynamicInstructionListTail, nullptr);

  // For now do not handle any dependences.
  this->handleRegisterDependence(DynamicInst, StaticInstruction);
  this->handleMemoryDependence(DynamicInst);
  this->handleControlDependence(DynamicInst);

  // Handle the memory base/offset.
  this->handleMemoryBase(DynamicInst);

  // Add to the list.
  if (this->DynamicInstructionListTail != nullptr) {
    this->DynamicInstructionListTail->Next = DynamicInst;
  }
  this->DynamicInstructionListTail = DynamicInst;
  if (this->DynamicInstructionListHead == nullptr) {
    this->DynamicInstructionListHead = DynamicInst;
  }
  // Add the map from static instrunction to dynamic.
  if (this->StaticToDynamicMap.find(StaticInstruction) ==
      this->StaticToDynamicMap.end()) {
    this->StaticToDynamicMap.emplace(StaticInstruction,
                                     std::list<DynamicInstruction*>());
  }
  this->StaticToDynamicMap.at(StaticInstruction).push_back(DynamicInst);
}

void DataGraph::addRegisterDependence(DynamicInstruction* DynamicInst,
                                      llvm::Instruction* OperandStaticInst) {
  // Push the latest dynamic inst of the operand static inst.
  DynamicInstruction* OperandDynamicInst =
      this->getLatestDynamicIdForStaticInstruction(OperandStaticInst);
  this->RegDeps[DynamicInst].insert(OperandDynamicInst);
}

void DataGraph::handleRegisterDependence(DynamicInstruction* DynamicInst,
                                         llvm::Instruction* StaticInstruction) {
  // Handle register dependence.
  assert(this->RegDeps.find(DynamicInst) == this->RegDeps.end() &&
         "This dynamic instruction's register dependence has already been "
         "handled.");
  this->RegDeps.emplace(DynamicInst, std::unordered_set<DynamicInstruction*>());

  // Special rule for phi node.
  if (auto PhiStaticInst = llvm::dyn_cast<llvm::PHINode>(StaticInstruction)) {
    // Get the previous basic block.
    DynamicInstruction* PrevNonPhiDYnamicInstruction =
        this->getPreviousNonPhiDynamicInstruction(DynamicInst);
    assert(PrevNonPhiDYnamicInstruction != nullptr &&
           "There should be a non-phi inst before phi inst.");
    llvm::BasicBlock* PrevBasicBlock =
        PrevNonPhiDYnamicInstruction->getStaticInstruction()->getParent();
    // Check all the incoming values.
    for (unsigned int OperandId = 0,
                      NumOperands = PhiStaticInst->getNumIncomingValues();
         OperandId != NumOperands; ++OperandId) {
      if (PhiStaticInst->getIncomingBlock(OperandId) == PrevBasicBlock) {
        // We found the previous block.
        // Check if the value is produced by an inst.
        if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
                PhiStaticInst->getIncomingValue(OperandId))) {
          // Also resolve the phi dependence for phi node.
          OperandStaticInst =
              this->resolveRegisterDependenceInPhiNode(OperandStaticInst);
          if (OperandStaticInst != nullptr) {
            // Sanity check: no instruction can be dynamically dependent on
            // phi node.
            assert(!llvm::isa<llvm::PHINode>(OperandStaticInst) &&
                   "No inst should be dynamically dependent on phi node.");
            this->addRegisterDependence(DynamicInst, OperandStaticInst);
          }
        }
        break;
      }
    }
    return;
  }

  for (unsigned int OperandId = 0,
                    NumOperands = StaticInstruction->getNumOperands();
       OperandId != NumOperands; ++OperandId) {
    if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
            StaticInstruction->getOperand(OperandId))) {
      OperandStaticInst =
          this->resolveRegisterDependenceInPhiNode(OperandStaticInst);
      // Just insert the register dependence.
      if (OperandStaticInst != nullptr) {
        // Sanity check: no instruction can be dynamically dependent on
        // phi node.
        assert(!llvm::isa<llvm::PHINode>(OperandStaticInst) &&
               "No inst should be dynamically dependent on phi node.");
        this->addRegisterDependence(DynamicInst, OperandStaticInst);
      }
    }
  }
}

llvm::Instruction* DataGraph::resolveRegisterDependenceInPhiNode(
    llvm::Instruction* OperandStaticInst) {
  // Special rules to remove phi dependence.
  // If the instruction is dependent on a phi node, we set the
  // register dependence to the dependent of phi node.
  // This helps in replay to skip the phi node. But we keep the
  // phi node in the graph for now so that the dynamic id is still valid.
  while (OperandStaticInst != nullptr &&
         llvm::isa<llvm::PHINode>(OperandStaticInst)) {
    // Assert that the phi node should have at most one dynamic dependence.
    auto LastDependentPhiDynamicInst =
        this->StaticToDynamicMap.at(OperandStaticInst).back();
    if (this->RegDeps.find(LastDependentPhiDynamicInst) !=
        this->RegDeps.end()) {
      const auto& PhiDependentSet =
          this->RegDeps.at(LastDependentPhiDynamicInst);
      assert(PhiDependentSet.size() <= 1 &&
             "Dependent Phi node should have at most one dynamic dependence.");
      if (PhiDependentSet.size() == 1) {
        OperandStaticInst = (*PhiDependentSet.begin())->getStaticInstruction();
      } else {
        // There is no dependence after all the phi node is resovled.
        OperandStaticInst = nullptr;
      }
    }
  }
  return OperandStaticInst;
}

bool DataGraph::checkAndAddMemoryDependence(
    std::unordered_map<uint64_t, DynamicInstruction*>& LastMap, uint64_t Addr,
    DynamicInstruction* DynamicInst) {
  auto Iter = LastMap.find(Addr);
  if (Iter != LastMap.end()) {
    // There is a dependence.
    auto DepIter = this->MemDeps.find(DynamicInst);
    if (DepIter == this->MemDeps.end()) {
      DepIter =
          this->MemDeps
              .emplace(DynamicInst, std::unordered_set<DynamicInstruction*>())
              .first;
    }
    DepIter->second.insert(Iter->second);
    return true;
  }
  return false;
}

void DataGraph::handleMemoryDependence(DynamicInstruction* DynamicInst) {
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->getStaticInstruction())) {
    // Handle RAW dependence.
    assert(DynamicInst->DynamicOperands.size() == 1 &&
           "Invalid number of dynamic operands for load.");
    uint64_t BaseAddr =
        std::stoull(DynamicInst->DynamicOperands[0]->Value, nullptr, 16);
    llvm::Type* LoadedType =
        LoadStaticInstruction->getPointerOperandType()->getPointerElementType();
    uint64_t LoadedTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(LoadedType);
    for (uint64_t ByteIter = 0; ByteIter < LoadedTypeSizeInByte; ++ByteIter) {
      uint64_t Addr = BaseAddr + ByteIter;
      if (this->checkAndAddMemoryDependence(this->AddrToLastStoreInstMap, Addr,
                                            DynamicInst)) {
        this->NumMemDependences++;
      }
      // Update the latest read map.
      this->AddrToLastLoadInstMap.emplace(Addr, DynamicInst);
    }
    return;
  }

  if (auto StoreStaticInstruction = llvm::dyn_cast<llvm::StoreInst>(
          DynamicInst->getStaticInstruction())) {
    // Handle WAW and WAR dependence.
    assert(DynamicInst->DynamicOperands.size() == 2 &&
           "Invalid number of dynamic operands for store.");
    uint64_t BaseAddr =
        std::stoull(DynamicInst->DynamicOperands[1]->Value, nullptr, 16);
    llvm::Type* StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(StoredType);
    for (uint64_t ByteIter = 0; ByteIter < StoredTypeSizeInByte; ++ByteIter) {
      uint64_t Addr = BaseAddr + ByteIter;
      // WAW.
      if (this->checkAndAddMemoryDependence(this->AddrToLastStoreInstMap, Addr,
                                            DynamicInst)) {
        this->NumMemDependences++;
      }
      // WAR.
      if (this->checkAndAddMemoryDependence(this->AddrToLastLoadInstMap, Addr,
                                            DynamicInst)) {
        this->NumMemDependences++;
      }
      // Update the last store map.
      this->AddrToLastStoreInstMap.emplace(Addr, DynamicInst);
    }
    return;
  }
}

void DataGraph::parseFunctionEnter(TraceParser::TracedFuncEnter& Parsed) {
  llvm::Function* StaticFunction = this->Module->getFunction(Parsed.Func);
  assert(StaticFunction && "Failed to loop up traced function in module.");

  // Handle the frame stack.
  std::unordered_map<llvm::Argument*, DynamicValue*> DynamicFrame;
  size_t ParsedArgumentIndex = 0;
  unsigned int ArgumentIndex = 0;
  auto ArgumentIter = StaticFunction->arg_begin();
  auto ArgumentEnd = StaticFunction->arg_end();
  while (ParsedArgumentIndex != Parsed.Arguments.size() &&
         ArgumentIter != ArgumentEnd) {
    DynamicValue* DynamicArgument =
        new DynamicValue(Parsed.Arguments[ParsedArgumentIndex]);

    llvm::Argument* StaticArgument = &*ArgumentIter;
    DynamicFrame.emplace(StaticArgument, DynamicArgument);

    // If there is a previous call inst, copy the base/offset.
    // Otherwise, initialize to itself.
    if (this->DynamicFrameStack.size() == 0) {
      // There is no caller.
      DynamicArgument->MemBase = StaticArgument->getName();
      DynamicArgument->MemOffset = 0;
    } else {
      // The previous inst must be a call.
      DynamicInstruction* PrevDynamicInstruction =
          this->DynamicInstructionListTail;
      assert(llvm::isa<llvm::CallInst>(
                 PrevDynamicInstruction->getStaticInstruction()) &&
             "The previous instruction is not a call");
      DynamicArgument->MemBase =
          PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemBase;
      DynamicArgument->MemOffset =
          PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemOffset;
    }

    ++ParsedArgumentIndex;
    ++ArgumentIter;
    ++ArgumentIndex;
  }
  assert(ArgumentIter == ArgumentEnd &&
         ParsedArgumentIndex == Parsed.Arguments.size() &&
         "Unmatched number of arguments and value lines in function enter.");

  // ---
  // Start modification.
  // ---

  // Set the current function.
  this->CurrentFunctionName = Parsed.Func;
  this->CurrentFunction = StaticFunction;
  this->CurrentBasicBlockName = "";
  this->CurrentIndex = -1;

  // Push the new frame.
  this->DynamicFrameStack.push_front(std::move(DynamicFrame));
}

llvm::Instruction* DataGraph::getLLVMInstruction(
    const std::string& FunctionName, const std::string& BasicBlockName,
    const int Index) {
  if (FunctionName != this->CurrentFunctionName) {
    // Pop the frame stack.
    this->DynamicFrameStack.pop_front();
    // Assert there is still at least one frame left.
    assert(!this->DynamicFrameStack.empty() && "Empty frame stack.");

    this->CurrentFunctionName = FunctionName;
    this->CurrentFunction =
        this->Module->getFunction(this->CurrentFunctionName);
    assert(this->CurrentFunction &&
           "Failed to loop up traced function in module.");

    // Remember to clear the BB and Index.
    this->CurrentBasicBlock = nullptr;
    this->CurrentBasicBlockName = "";
    this->CurrentIndex = -1;
  }
  assert(FunctionName == this->CurrentFunctionName && "Invalid function name.");

  if (BasicBlockName != this->CurrentBasicBlockName) {
    // We are in a new basic block.
    // Find the new basic block.
    for (auto BBIter = this->CurrentFunction->begin(),
              BBEnd = this->CurrentFunction->end();
         BBIter != BBEnd; ++BBIter) {
      if (std::string(BBIter->getName()) == BasicBlockName) {
        this->CurrentBasicBlock = &*BBIter;
        this->CurrentBasicBlockName = BasicBlockName;
        // Clear index.
        this->CurrentIndex = -1;
        break;
      }
    }
  }
  assert(BasicBlockName == this->CurrentBasicBlockName &&
         "Invalid basic block name.");

  if (Index != this->CurrentIndex) {
    int Count = 0;
    for (auto InstIter = this->CurrentBasicBlock->begin(),
              InstEnd = this->CurrentBasicBlock->end();
         InstIter != InstEnd; ++InstIter) {
      if (Count == Index) {
        this->CurrentIndex = Index;
        this->CurrentStaticInstruction = InstIter;
        break;
      }
      Count++;
    }
  }
  assert(Index == this->CurrentIndex && "Invalid index.");

  assert(this->CurrentStaticInstruction != this->CurrentBasicBlock->end() &&
         "Invalid end index.");

  llvm::Instruction* StaticInstruction = &*this->CurrentStaticInstruction;
  // Update the current index.
  this->CurrentIndex++;
  this->CurrentStaticInstruction++;
  return StaticInstruction;
}

DynamicInstruction* DataGraph::getLatestDynamicIdForStaticInstruction(
    llvm::Instruction* StaticInstruction) {
  // Push the latest dynamic inst of the operand static inst.
  if (this->StaticToDynamicMap.find(StaticInstruction) ==
      this->StaticToDynamicMap.end()) {
    DEBUG(llvm::errs() << "operand inst "
                       << StaticInstruction->getFunction()->getName()
                       << StaticInstruction->getParent()->getName()
                       << StaticInstruction->getName() << '\n');
  }
  assert((this->StaticToDynamicMap.find(StaticInstruction) !=
          this->StaticToDynamicMap.end()) &&
         "Static inst should have dynamic occurrence.");
  const auto& DynamicInsts = this->StaticToDynamicMap.at(StaticInstruction);
  assert(DynamicInsts.size() != 0 &&
         "Static inst should have at least one dynamic occurrence.");
  return DynamicInsts.back();
}

void DataGraph::handleMemoryBase(DynamicInstruction* DynamicInst) {
  llvm::Instruction* StaticInstruction = DynamicInst->getStaticInstruction();

  // DEBUG(llvm::errs() << "handle memory base for inst "
  //                    << StaticInstruction->getFunction()->getName()
  //                    << StaticInstruction->getParent()->getName()
  //                    << StaticInstruction->getName() << '\n');

  // Special rule for phi node.
  if (auto PhiStaticInst = llvm::dyn_cast<llvm::PHINode>(StaticInstruction)) {
    // Get the previous basic block.

    DynamicInstruction* PrevNonPhiDynamicInstruction =
        this->getPreviousNonPhiDynamicInstruction(DynamicInst);

    assert(PrevNonPhiDynamicInstruction != nullptr &&
           "There should have at least one dynamic instruction before phi "
           "instruction.");

    llvm::BasicBlock* PrevBasicBlock =
        PrevNonPhiDynamicInstruction->getStaticInstruction()->getParent();
    // Check all the incoming values.
    for (unsigned int OperandIndex = 0,
                      NumOperands = PhiStaticInst->getNumIncomingValues();
         OperandIndex != NumOperands; ++OperandIndex) {
      if (PhiStaticInst->getIncomingBlock(OperandIndex) == PrevBasicBlock) {
        // We found the previous block.
        // Check if the value is produced by an inst.
        if (auto OperandStaticInstruction = llvm::dyn_cast<llvm::Instruction>(
                PhiStaticInst->getIncomingValue(OperandIndex))) {
          // Simply populates the result from the previous dynamic inst.
          DynamicInstruction* OperandDynamicInstruction =
              this->getLatestDynamicIdForStaticInstruction(
                  OperandStaticInstruction);
          DynamicInst->DynamicOperands[OperandIndex]->MemBase =
              OperandDynamicInstruction->DynamicResult->MemBase;
          DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
              OperandDynamicInstruction->DynamicResult->MemOffset;
        } else if (auto OperandStaticArgument = llvm::dyn_cast<llvm::Argument>(
                       PhiStaticInst->getIncomingValue(OperandIndex))) {
          // This operand comes from an argument.
          // Look up into the DynamicFrameStack to get the value.
          const auto& DynamicFrame = this->DynamicFrameStack.front();
          auto FrameIter = DynamicFrame.find(OperandStaticArgument);
          assert(FrameIter != DynamicFrame.end() &&
                 "Failed to find argument in the frame.");
          DynamicInst->DynamicOperands[OperandIndex]->MemBase =
              FrameIter->second->MemBase;
          DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
              FrameIter->second->MemOffset;
        } else {
          // Looks like this operand comes from an constant.
          // Is it OK for us to sliently ignore this case?
        }

        // Copy base/offset to the output.
        DynamicInst->DynamicResult->MemBase =
            DynamicInst->DynamicOperands[OperandIndex]->MemBase;
        DynamicInst->DynamicResult->MemOffset =
            DynamicInst->DynamicOperands[OperandIndex]->MemOffset;

        break;
      }
    }
    return;
  }

  // First initialize the memory base/offset for all the operands.
  for (unsigned int OperandIndex = 0,
                    NumOperands = StaticInstruction->getNumOperands();
       OperandIndex < NumOperands; ++OperandIndex) {
    assert(DynamicInst->DynamicOperands[OperandIndex] != nullptr &&
           "Unknown dynamic value for operand.");
    llvm::Value* Operand = StaticInstruction->getOperand(OperandIndex);
    if (auto OperandStaticInstruction =
            llvm::dyn_cast<llvm::Instruction>(Operand)) {
      // This operand comes from an instruction.
      // Simply populates the result from the previous dynamic inst.
      DynamicInstruction* OperandDynamicInstruction =
          this->getLatestDynamicIdForStaticInstruction(
              OperandStaticInstruction);
      DynamicInst->DynamicOperands[OperandIndex]->MemBase =
          OperandDynamicInstruction->DynamicResult->MemBase;
      DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
          OperandDynamicInstruction->DynamicResult->MemOffset;
    } else if (auto OperandStaticArgument =
                   llvm::dyn_cast<llvm::Argument>(Operand)) {
      // This operand comes from an argument.
      // Look up into the DynamicFrameStack to get the value.
      const auto& DynamicFrame = this->DynamicFrameStack.front();
      auto FrameIter = DynamicFrame.find(OperandStaticArgument);
      assert(FrameIter != DynamicFrame.end() &&
             "Failed to find argument in the frame.");
      DynamicInst->DynamicOperands[OperandIndex]->MemBase =
          FrameIter->second->MemBase;
      DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
          FrameIter->second->MemOffset;
    } else {
      // Looks like this operand comes from an constant.
      // Is it OK for us to sliently ignore this case?
    }
  }

  // Compute the base/offset for the result of the instruction we actually
  // care about. NOTE: A complete support to memory base offset computation is
  // not trivial if we consider the numerious way an memory address can be
  // generated. Consider the following example: i64 i = load ... i64 ii = add
  // i, x i32* pi = bitcase ii ... This example is essential impossible to
  // handle if x is also a address or something. To make things worse:
  // consider the inter-procedure base/offset computation. We now only support
  // a few ways of generated address. We hope the user would not do something
  // crazy in the source code.
  if (auto GEPStaticInstruction =
          llvm::dyn_cast<llvm::GetElementPtrInst>(StaticInstruction)) {
    // GEP.
    DynamicValue* BaseValue = DynamicInst->DynamicOperands[0];
    uint64_t OperandAddr = stoull(BaseValue->Value, nullptr, 16);
    uint64_t ResultAddr =
        stoull(DynamicInst->DynamicResult->Value, nullptr, 16);
    // Compute base/offset from the first operand.
    DynamicInst->DynamicResult->MemBase = BaseValue->MemBase;
    DynamicInst->DynamicResult->MemOffset =
        BaseValue->MemOffset + (ResultAddr - OperandAddr);

    {
      static int count = 0;
      if (count < 5) {
        DEBUG(llvm::errs() << "GEP " << DynamicInst->DynamicResult->MemBase
                           << ' ' << DynamicInst->DynamicResult->MemOffset
                           << ' ' << DynamicInst->DynamicResult->Value << ' '
                           << ResultAddr << ' ' << OperandAddr << ' '
                           << BaseValue->MemOffset << '\n');
        count++;
      }
    }
    return;
  }

  if (auto AllocaStaticInstruction =
          llvm::dyn_cast<llvm::AllocaInst>(StaticInstruction)) {
    // Alloca is easy, set the base/offset to itself.
    DynamicInst->DynamicResult->MemBase = AllocaStaticInstruction->getName();
    DynamicInst->DynamicResult->MemOffset = 0;
    return;
  }

  if (auto BitCastStaticInstruction =
          llvm::dyn_cast<llvm::BitCastInst>(StaticInstruction)) {
    // Bit cast is simple so we support it here.
    DynamicValue* BaseValue = DynamicInst->DynamicOperands[0];
    DynamicInst->DynamicResult->MemBase = BaseValue->MemBase;
    DynamicInst->DynamicResult->MemOffset = BaseValue->MemOffset;
    return;
  }

  // To have some basic support of dynamic memory access.
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(StaticInstruction)) {
    // We only worried about this if we are loading an pointer.
    if (LoadStaticInstruction->getType()->isPointerTy()) {
      // In that case, we init the base to itself, offset to 0.
      DynamicInst->DynamicResult->MemBase = LoadStaticInstruction->getName();
      DynamicInst->DynamicResult->MemOffset = 0;

      {
        static int count = 0;
        if (count < 5) {
          DEBUG(llvm::errs()
                << "LOAD " << DynamicInst->DynamicResult->MemBase << ' '
                << DynamicInst->DynamicResult->MemOffset << ' '
                << DynamicInst->DynamicResult->Value << ' ' << '\n');
          count++;
        }
      }

      return;
    }
  }
}

void DataGraph::handleControlDependence(DynamicInstruction* DynamicInst) {
  auto DepInst = this->getPreviousBranchDynamicInstruction(DynamicInst);
  if (DepInst != nullptr) {
    // DEBUG(llvm::errs() << "Get control dependence!\n");
    if (this->CtrDeps.find(DynamicInst) == this->CtrDeps.end()) {
      this->CtrDeps.emplace(DynamicInst,
                            std::unordered_set<DynamicInstruction*>());
    }
    this->CtrDeps.at(DynamicInst).insert(DepInst);
  }
}

DynamicInstruction* DataGraph::getPreviousDynamicInstruction() {
  if (this->DynamicFrameStack.empty()) {
    return nullptr;
  }
  {
    auto PrevStaticInstruction = this->CurrentStaticInstruction;
    --PrevStaticInstruction;
    return this->getLatestDynamicIdForStaticInstruction(
        &*PrevStaticInstruction);
  }
}

DynamicInstruction* DataGraph::getPreviousNonPhiDynamicInstruction(
    DynamicInstruction* DynamicInst) {
  while (DynamicInst != nullptr && DynamicInst->Prev != nullptr) {
    DynamicInstruction* DInstruction = DynamicInst->Prev;
    if (!llvm::isa<llvm::PHINode>(DInstruction->getStaticInstruction())) {
      return DInstruction;
    }
    DynamicInst = DynamicInst->Prev;
  }
  return nullptr;
}

DynamicInstruction* DataGraph::getPreviousBranchDynamicInstruction(
    DynamicInstruction* DynamicInst) {
  while (DynamicInst != nullptr && DynamicInst->Prev != nullptr) {
    DynamicInstruction* DInstruction = DynamicInst->Prev;
    switch (DInstruction->getStaticInstruction()->getOpcode()) {
      case llvm::Instruction::Br:
      case llvm::Instruction::Ret:
      case llvm::Instruction::Call:
        return DInstruction;
      default:
        break;
    }
    DynamicInst = DynamicInst->Prev;
  }
  return nullptr;
}

#undef DEBUG_TYPE