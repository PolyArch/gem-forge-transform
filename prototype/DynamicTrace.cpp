#include "DynamicTrace.h"
#include "GZUtil.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

#define DEBUG_TYPE "DynamicTrace"

DynamicValue::DynamicValue(const std::string& _Value)
    : Value(_Value), MemBase(""), MemOffset(0) {}

DynamicInstruction::DynamicInstruction(
    llvm::Instruction* _StaticInstruction, DynamicValue* _DynamicResult,
    std::vector<DynamicValue*> _DynamicOperands, DynamicInstruction* _Prev,
    DynamicInstruction* _Next)
    : StaticInstruction(_StaticInstruction),
      DynamicResult(_DynamicResult),
      DynamicOperands(std::move(_DynamicOperands)),
      Prev(_Prev),
      Next(_Next) {
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

DynamicTrace::DynamicTrace(const std::string& _TraceFileName,
                           llvm::Module* _Module)
    : DynamicInstructionListHead(nullptr),
      DynamicInstructionListTail(nullptr),
      Module(_Module),
      NumMemDependences(0),
      CurrentFunctionName(""),
      CurrentBasicBlockName(""),
      CurrentIndex(-1) {
  this->DataLayout = new llvm::DataLayout(this->Module);

  // Parse the trace file.
  GZTrace* TraceFile = gzOpenGZTrace(_TraceFileName.c_str());
  assert(TraceFile != NULL && "Failed to open trace file.");
  std::string Line;
  std::list<std::string> LineBuffer;
  uint64_t LineCount = 0;
  // while (std::getline(TraceFile, Line)) {
  while (true) {
    int LineLen = gzReadNextLine(TraceFile);
    if (LineLen == 0) {
      // Reached the end.
      break;
    }

    assert(LineLen > 0 && "Something wrong when reading gz trace file.");

    // Copy the new line to "Line" variable.
    Line = TraceFile->LineBuf;
    LineCount++;
    assert(LineLen == Line.size() && "Unmatched LineLen with Line.size().");
    // DEBUG(llvm::errs() << "Read in line # " << LineCount << ": " << Line
    //                    << '\n');
    // DEBUG(llvm::errs() << "Head " << TraceFile->HeadIdx << " Tail "
    //                    << TraceFile->TailIdx << '\n');

    if (isBreakLine(Line)) {
      // Time to parse the buffer.
      this->parseLineBuffer(LineBuffer);
      // Clear the buffer.
      LineBuffer.clear();
    }
    // Add the new line into the buffer if not empty.
    if (Line.size() > 0) {
      LineBuffer.push_back(Line);
    }
  }
  // Process the last instruction if there is any.
  if (LineBuffer.size() != 0) {
    this->parseLineBuffer(LineBuffer);
    LineBuffer.clear();
  }
  // TraceFile.close();
  gzCloseGZTrace(TraceFile);
}

DynamicTrace::~DynamicTrace() {
  DynamicInstruction* Iter = this->DynamicInstructionListHead;
  while (Iter != nullptr) {
    auto Prev = Iter;
    Iter = Iter->Next;
    delete Prev;
  }
  this->DynamicInstructionListHead = nullptr;
  this->DynamicInstructionListTail = nullptr;
  delete this->DataLayout;
}

void DynamicTrace::parseLineBuffer(const std::list<std::string>& LineBuffer) {
  // Silently ignore empty line buffer.
  if (LineBuffer.size() == 0) {
    return;
  }
  switch (LineBuffer.front().front()) {
    case 'i':
      // This is a dynamic instruction.
      this->parseDynamicInstruction(LineBuffer);
      break;
    case 'e':
      // Special case for function enter node.
      this->parseFunctionEnter(LineBuffer);
      break;
    default:
      llvm_unreachable("Unrecognized line buffer.");
  }
}

void DynamicTrace::parseDynamicInstruction(
    const std::list<std::string>& LineBuffer) {
  assert(LineBuffer.size() > 0 && "Emptry line buffer.");

  auto LineIter = LineBuffer.begin();
  // Parse the instruction line.
  auto InstructionLineFields = splitByChar(*LineIter, '|');
  assert(InstructionLineFields.size() == 5 &&
         "Incorrect fields for instrunction line.");
  assert(InstructionLineFields[0] == "i" && "The first filed must be \"i\".");
  const std::string& FunctionName = InstructionLineFields[1];
  const std::string& BasicBlockName = InstructionLineFields[2];
  int Index = std::stoi(InstructionLineFields[3]);
  llvm::Instruction* StaticInstruction =
      this->getLLVMInstruction(FunctionName, BasicBlockName, Index);
  if (InstructionLineFields[4] != StaticInstruction->getOpcodeName()) {
    DEBUG(llvm::errs() << "Unmatched static opcode "
                       << StaticInstruction->getOpcodeName()
                       << " dynamic opcode " << InstructionLineFields[4]);
  }
  assert(InstructionLineFields[4] == StaticInstruction->getOpcodeName() &&
         "Unmatched opcode.");

  // Parse the dynamic values.
  ++LineIter;
  DynamicValue* DynamicResult = nullptr;
  std::vector<DynamicValue*> DynamicOperands(
      StaticInstruction->getNumOperands(), nullptr);
  unsigned int OperandIndex = 0;
  while (LineIter != LineBuffer.end()) {
    switch (LineIter->at(0)) {
      case 'r': {
        // This is the result line.
        assert(DynamicResult == nullptr &&
               "Multiple results for single instruction.");
        auto ResultLineFields = splitByChar(*LineIter, '|');
        DynamicResult = new DynamicValue(ResultLineFields[3]);
        break;
      }
      case 'p': {
        assert(OperandIndex < StaticInstruction->getNumOperands() &&
               "Too much operands.");
        auto OperandLineFields = splitByChar(*LineIter, '|');
        if (OperandLineFields.size() < 4) {
          DEBUG(llvm::errs() << *LineIter << '\n');
        }
        assert(OperandLineFields.size() >= 4 &&
               "Too few fields for operand line.");
        DynamicOperands[OperandIndex++] =
            new DynamicValue(OperandLineFields[3]);
        break;
      }
      default: {
        llvm_unreachable("Unknown value line.");
        break;
      }
    }
    ++LineIter;
  }

  assert(OperandIndex == StaticInstruction->getNumOperands() &&
         "Missing dynamic value for operand.");

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
  if (llvm::isa<llvm::ReturnInst>(StaticInstruction) &&
      DynamicResult != nullptr) {
    // Free the dynamic result for now.
    delete DynamicResult;
    DynamicResult = nullptr;
  }

  // Create the new dynamic instructions.
  DynamicInstruction* DynamicInst = new DynamicInstruction(
      StaticInstruction, DynamicResult, std::move(DynamicOperands),
      this->DynamicInstructionListTail, nullptr);

  // For now do not handle any dependences.
  this->handleRegisterDependence(DynamicInst, StaticInstruction);
  this->handleMemoryDependence(DynamicInst);

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

void DynamicTrace::addRegisterDependence(DynamicInstruction* DynamicInst,
                                         llvm::Instruction* OperandStaticInst) {
  // Push the latest dynamic inst of the operand static inst.
  DynamicInstruction* OperandDynamicInst =
      this->getLatestDynamicIdForStaticInstruction(OperandStaticInst);
  this->RegDeps[DynamicInst].insert(OperandDynamicInst);
}

void DynamicTrace::handleRegisterDependence(
    DynamicInstruction* DynamicInst, llvm::Instruction* StaticInstruction) {
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
        PrevNonPhiDYnamicInstruction->StaticInstruction->getParent();
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

llvm::Instruction* DynamicTrace::resolveRegisterDependenceInPhiNode(
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
        OperandStaticInst = (*PhiDependentSet.begin())->StaticInstruction;
      } else {
        // There is no dependence after all the phi node is resovled.
        OperandStaticInst = nullptr;
      }
    }
  }
  return OperandStaticInst;
}

bool DynamicTrace::checkAndAddMemoryDependence(
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

void DynamicTrace::handleMemoryDependence(DynamicInstruction* DynamicInst) {
  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->StaticInstruction)) {
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

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(DynamicInst->StaticInstruction)) {
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

void DynamicTrace::parseFunctionEnter(
    const std::list<std::string>& LineBuffer) {
  // TODO: Parse function enter. Handle call stack

  assert(LineBuffer.size() > 0 && "Emptry line buffer.");

  auto LineIter = LineBuffer.begin();
  // Parse the enter line.
  auto EnterLineFields = splitByChar(*LineIter, '|');
  assert(EnterLineFields.size() == 2 &&
         "Incorrect fields for function enter line.");
  assert(EnterLineFields[0] == "e" && "The first filed must be \"e\".");

  llvm::Function* StaticFunction =
      this->Module->getFunction(EnterLineFields[1]);
  assert(StaticFunction && "Failed to loop up traced function in module.");

  // Handle the frame stack.
  std::unordered_map<llvm::Argument*, DynamicValue*> DynamicFrame;
  ++LineIter;
  unsigned int ArgumentIndex = 0;
  auto ArgumentIter = StaticFunction->arg_begin();
  auto ArgumentEnd = StaticFunction->arg_end();
  while (LineIter != LineBuffer.end() && ArgumentIter != ArgumentEnd) {
    switch (LineIter->at(0)) {
      case 'p': {
        auto ArugmentLineFields = splitByChar(*LineIter, '|');
        assert(ArugmentLineFields.size() >= 4 &&
               "Too few argument line fields.");
        DynamicValue* DynamicArgument = new DynamicValue(ArugmentLineFields[3]);

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
                     PrevDynamicInstruction->StaticInstruction) &&
                 "The previous instruction is not a call");
          DynamicArgument->MemBase =
              PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemBase;
          DynamicArgument->MemOffset =
              PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemOffset;
        }
        break;
      }
      default: {
        llvm_unreachable("Unknown value line for function enter.");
        break;
      }
    }
    ++LineIter;
    ++ArgumentIter;
    ++ArgumentIndex;
  }
  assert(ArgumentIter == ArgumentEnd && LineIter == LineBuffer.end() &&
         "Unmatched number of arguments and value lines in function enter.");

  // ---
  // Start modification.
  // ---

  // Set the current function.
  this->CurrentFunctionName = EnterLineFields[1];
  this->CurrentFunction = StaticFunction;
  this->CurrentBasicBlockName = "";
  this->CurrentIndex = -1;

  // Push the new frame.
  this->DynamicFrameStack.push_front(std::move(DynamicFrame));
}

llvm::Instruction* DynamicTrace::getLLVMInstruction(
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

DynamicInstruction* DynamicTrace::getLatestDynamicIdForStaticInstruction(
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

void DynamicTrace::handleMemoryBase(DynamicInstruction* DynamicInst) {
  llvm::Instruction* StaticInstruction = DynamicInst->StaticInstruction;

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
        PrevNonPhiDynamicInstruction->StaticInstruction->getParent();
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

  // Compute the base/offset for the result of the instruction we actually care
  // about.
  // NOTE: A complete support to memory base offset computation is not trivial
  // if we consider the numerious way an memory address can be generated.
  // Consider the following example:
  // i64 i = load ...
  // i64 ii = add i, x
  // i32* pi = bitcase ii ...
  // This example is essential impossible to handle if x is also a address or
  // something.
  // To make things worse: consider the inter-procedure base/offset computation.
  // We now only support a few ways of generated address. We hope the
  // user would not do something crazy in the source code.
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
}

DynamicInstruction* DynamicTrace::getPreviousDynamicInstruction() {
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

DynamicInstruction* DynamicTrace::getPreviousNonPhiDynamicInstruction(
    DynamicInstruction* DynamicInst) {
  while (DynamicInst != nullptr && DynamicInst->Prev != nullptr) {
    DynamicInstruction* DInstruction = DynamicInst->Prev;
    if (!llvm::isa<llvm::PHINode>(DInstruction->StaticInstruction)) {
      return DInstruction;
    }
    DynamicInst = DynamicInst->Prev;
  }
  return nullptr;
}

#undef DEBUG_TYPE