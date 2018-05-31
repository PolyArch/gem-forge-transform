#include "DataGraph.h"
#include "TraceParserGZip.h"
#include "TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

static llvm::cl::opt<std::string> TraceFileName("trace-file",
                                                llvm::cl::desc("Trace file."));
static llvm::cl::opt<std::string> TraceFileFormat(
    "trace-format", llvm::cl::desc("Trace file format."));

#define DEBUG_TYPE "DataGraph"

DynamicValue::DynamicValue(const std::string& _Value)
    : Value(_Value), MemBase(""), MemOffset(0) {}

DynamicInstruction::DynamicInstruction()
    : Id(allocateId()), DynamicResult(nullptr), Prev(nullptr), Next(nullptr) {}

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

DataGraph::DynamicFrame::DynamicFrame(
    llvm::Function* _Function,
    std::unordered_map<llvm::Argument*, DynamicValue*>&& _Arguments)
    : Function(_Function),
      Arguments(std::move(_Arguments)),
      PrevBasicBlock(nullptr),
      PrevBranchInstId(0) {}

DataGraph::DynamicFrame::~DynamicFrame() {
  // Remember to release the dynamic values;
  for (auto& Entry : this->Arguments) {
    delete Entry.second;
    Entry.second = nullptr;
  }
}

DynamicValue* DataGraph::DynamicFrame::getArgValue(llvm::Argument* Arg) const {
  auto Iter = this->Arguments.find(Arg);
  assert(Iter != this->Arguments.end() &&
         "Failed to find argument in the frame.");
  return Iter->second;
}

DataGraph::DataGraph(llvm::Module* _Module)
    : DynamicInstructionListHead(nullptr),
      DynamicInstructionListTail(nullptr),
      Module(_Module),
      NumMemDependences(0),
      Parser(nullptr),
      CurrentBasicBlockName(""),
      CurrentIndex(-1) {
  assert(TraceFileName.getNumOccurrences() == 1 &&
         "Please specify the trace file.");

  if (TraceFileFormat.getNumOccurrences() == 0 ||
      TraceFileFormat.getValue() == "gzip") {
    this->Parser = new TraceParserGZip(TraceFileName);
  } else if (TraceFileFormat.getValue() == "protobuf") {
    this->Parser = new TraceParserProtobuf(TraceFileName);
  } else {
    llvm_unreachable("Unknown trace file format.");
  }

  this->DataLayout = new llvm::DataLayout(this->Module);
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

DynamicInstruction* DataGraph::loadOneDynamicInst() {
  while (true) {
    auto NextType = this->Parser->getNextType();
    if (NextType == TraceParser::END) {
      return nullptr;
    }
    switch (NextType) {
      case TraceParser::INST: {
        // DEBUG(llvm::errs() << "Parse inst.\n");
        auto Parsed = this->Parser->parseLLVMInstruction();
        auto OldTail = this->DynamicInstructionListTail;
        this->parseDynamicInstruction(Parsed);
        if (OldTail != this->DynamicInstructionListTail) {
          // We have found something new.
          // return it.
          return this->DynamicInstructionListTail;
        }
        break;
      }
      case TraceParser::FUNC_ENTER: {
        // DEBUG(llvm::errs() << "Parse func enter.\n");
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
  return nullptr;
}

void DataGraph::commitOneDynamicInst() {
  assert(this->DynamicInstructionListHead != nullptr &&
         "No inst to be committed.");
  // Remove all the dependence information.
  auto Id = this->DynamicInstructionListHead->Id;
  this->RegDeps.erase(Id);
  this->MemDeps.erase(Id);
  this->CtrDeps.erase(Id);
  this->AliveDynamicInstsMap.erase(Id);
  auto Committed = this->DynamicInstructionListHead;
  this->DynamicInstructionListHead = this->DynamicInstructionListHead->Next;
  delete Committed;
}

void DataGraph::parseDynamicInstruction(TraceParser::TracedInst& Parsed) {
  llvm::Instruction* StaticInstruction =
      this->getLLVMInstruction(Parsed.Func, Parsed.BB, Parsed.Id);
  if (Parsed.Op != StaticInstruction->getOpcodeName()) {
    DEBUG(llvm::errs() << "Unmatched static "
                       << StaticInstruction->getFunction()->getName()
                       << "::" << StaticInstruction->getParent()->getName()
                       << "::" << StaticInstruction->getOpcodeName()
                       << " dynamic " << Parsed.Func << "::" << Parsed.BB
                       << "::" << Parsed.Op << '\n');
  }
  assert(Parsed.Op == StaticInstruction->getOpcodeName() &&
         "Unmatched opcode.\n");

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

  // Special case for phi node. Resolve it and not add it into the graph.
  if (auto StaticPhi = llvm::dyn_cast<llvm::PHINode>(StaticInstruction)) {
    this->handlePhiNode(StaticPhi, Parsed);
    return;
  }

  // Guarantee non phi node.
  // Create the new dynamic instructions.
  DynamicInstruction* DynamicInst = new LLVMDynamicInstruction(
      StaticInstruction, Parsed, this->DynamicInstructionListTail, nullptr);

  // Handle the dependence.
  this->handleRegisterDependence(DynamicInst, StaticInstruction);
  this->handleMemoryDependence(DynamicInst);
  this->handleControlDependence(DynamicInst);

  // Handle the memory base/offset.
  this->handleMemoryBase(DynamicInst);

  /***************************************************************************
   * Insert the instruction into the list.
   * Update the PrevBasicBlock.
   * If this is a ret, pop the frame stack.
   * Add to the list.
   * Add to the alive map.
   * Add to the static map.
   ***************************************************************************/
  // Update the previous block.
  this->DynamicFrameStack.front().PrevBasicBlock =
      StaticInstruction->getParent();

  // If this is a ret, pop the stack.
  if (llvm::isa<llvm::ReturnInst>(StaticInstruction)) {
    this->DynamicFrameStack.pop_front();
    // Remember to clear the BB and Index.
    this->CurrentBasicBlock = nullptr;
    this->CurrentBasicBlockName = "";
    this->CurrentIndex = -1;
  }

  // Add to the list.
  if (this->DynamicInstructionListTail != nullptr) {
    this->DynamicInstructionListTail->Next = DynamicInst;
  }
  this->DynamicInstructionListTail = DynamicInst;
  if (this->DynamicInstructionListHead == nullptr) {
    this->DynamicInstructionListHead = DynamicInst;
  }

  // Add to the alive map.
  this->AliveDynamicInstsMap[DynamicInst->Id] = DynamicInst;

  // Add the map from static instrunction to dynamic.
  this->StaticToLastDynamicMap[StaticInstruction] = DynamicInst->Id;
}

void DataGraph::handleRegisterDependence(DynamicInstruction* DynamicInst,
                                         llvm::Instruction* StaticInstruction) {
  // Handle register dependence.
  assert(this->RegDeps.find(DynamicInst->Id) == this->RegDeps.end() &&
         "This dynamic instruction's register dependence has already been "
         "handled.");

  assert(StaticInstruction->getOpcode() != llvm::Instruction::PHI &&
         "Can only handle register dependence for non phi node.");
  this->RegDeps.emplace(DynamicInst->Id, std::unordered_set<DynamicId>());

  for (unsigned int OperandId = 0,
                    NumOperands = StaticInstruction->getNumOperands();
       OperandId != NumOperands; ++OperandId) {
    if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
            StaticInstruction->getOperand(OperandId))) {
      // If this dependends on a phi node, check the phi node actually has
      // dependence.
      if (auto OperandStaticPhi =
              llvm::dyn_cast<llvm::PHINode>(OperandStaticInst)) {
        auto Iter = this->PhiNodeDependenceMap.find(OperandStaticPhi);
        if (Iter != this->PhiNodeDependenceMap.end()) {
          // We have found a dependence.
          this->RegDeps.at(DynamicInst->Id).insert(Iter->second);
        }
      } else {
        // non phi node dependence.
        auto Iter = this->StaticToLastDynamicMap.find(OperandStaticInst);
        assert(Iter != this->StaticToLastDynamicMap.end() &&
               "Failed to look up last dynamic for register dependence.");
        this->RegDeps.at(DynamicInst->Id).insert(Iter->second);
      }
    }
  }
}

void DataGraph::handlePhiNode(llvm::PHINode* StaticPhi,
                              const TraceParser::TracedInst& Parsed) {
  // Initialize the dynamic value.
  this->StaticToLastMemBaseOffsetMap.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(StaticPhi),
                                             std::forward_as_tuple("", 0));
  auto& PhiMemBaseOffset = this->StaticToLastMemBaseOffsetMap.at(StaticPhi);

  auto PrevBasicBlock = this->DynamicFrameStack.front().PrevBasicBlock;
  assert(PrevBasicBlock != nullptr && "Invalid previous basic block.");

  // Check all the incoming values.
  for (unsigned int OperandId = 0,
                    NumOperands = StaticPhi->getNumIncomingValues();
       OperandId != NumOperands; ++OperandId) {
    if (StaticPhi->getIncomingBlock(OperandId) == PrevBasicBlock) {
      // We found the previous block.
      // Check if the value is produced by an inst.
      if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
              StaticPhi->getIncomingValue(OperandId))) {
        // Update the memory base/offset by copying from the dependent one.
        PhiMemBaseOffset =
            this->getLatestMemBaseOffsetForStaticInstruction(OperandStaticInst);

        // Two cases:
        // 1. If the dependent inst is a phi node, look up the
        // PhiNodeDependenceMap.
        // 2. Otherwise, look up the StaticToLastDynamicMap.
        DynamicId DependentId;
        bool Found = false;
        if (auto OperandStaticPhi =
                llvm::dyn_cast<llvm::PHINode>(OperandStaticInst)) {
          auto Iter = this->PhiNodeDependenceMap.find(OperandStaticPhi);
          Found = Iter != this->PhiNodeDependenceMap.end();
          if (Found) {
            DependentId = Iter->second;
          }
        } else {
          // There must be a dynamic instance for this.
          assert(this->StaticToLastDynamicMap.find(OperandStaticInst) !=
                     this->StaticToLastDynamicMap.end() &&
                 "Missing dynamic instruction for phi incoming value.");
          Found = true;
          DependentId = this->StaticToLastDynamicMap.at(OperandStaticInst);
        }

        if (Found) {
          this->PhiNodeDependenceMap[StaticPhi] = DependentId;
        } else {
          this->PhiNodeDependenceMap.erase(StaticPhi);
        }
      } else if (auto OperandStaticArgument = llvm::dyn_cast<llvm::Argument>(
                     StaticPhi->getIncomingValue(OperandId))) {
        // This operand comes from an argument.
        // Look up into the DynamicFrameStack to get the value.
        const auto& DynamicFrame = this->DynamicFrameStack.front();
        auto DynamicValue = DynamicFrame.getArgValue(OperandStaticArgument);
        PhiMemBaseOffset.first = DynamicValue->MemBase;
        PhiMemBaseOffset.second = DynamicValue->MemOffset;
      } else {
        // Looks like this operand comes from an constant.
        // Is it OK for us to sliently ignore this case?
      }
      return;
    }
  }
  assert(false && "Failed to find an incoming block for phi node.");
}

bool DataGraph::checkAndAddMemoryDependence(
    std::unordered_map<uint64_t, DynamicId>& LastMap, uint64_t Addr,
    DynamicInstruction* DynamicInst) {
  auto Iter = LastMap.find(Addr);
  if (Iter != LastMap.end()) {
    // There is a dependence.
    auto DepIter = this->MemDeps.find(DynamicInst->Id);
    if (DepIter == this->MemDeps.end()) {
      DepIter = this->MemDeps
                    .emplace(DynamicInst->Id, std::unordered_set<DynamicId>())
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
      this->AddrToLastLoadInstMap.emplace(Addr, DynamicInst->Id);
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
      this->AddrToLastStoreInstMap.emplace(Addr, DynamicInst->Id);
    }
    return;
  }
}

void DataGraph::parseFunctionEnter(TraceParser::TracedFuncEnter& Parsed) {
  llvm::Function* StaticFunction = this->Module->getFunction(Parsed.Func);
  assert(StaticFunction && "Failed to loop up traced function in module.");

  // Handle the frame stack.
  std::unordered_map<llvm::Argument*, DynamicValue*> DynamicArguments;
  size_t ParsedArgumentIndex = 0;
  unsigned int ArgumentIndex = 0;
  auto ArgumentIter = StaticFunction->arg_begin();
  auto ArgumentEnd = StaticFunction->arg_end();
  while (ParsedArgumentIndex != Parsed.Arguments.size() &&
         ArgumentIter != ArgumentEnd) {
    DynamicValue* DynamicArgument =
        new DynamicValue(Parsed.Arguments[ParsedArgumentIndex]);

    llvm::Argument* StaticArgument = &*ArgumentIter;
    DynamicArguments.emplace(StaticArgument, DynamicArgument);

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

  // Recursion check.
  // Currently our framework does not support recursion yet. I just make an
  // assertion here. It requires a more powerful dynamic frame stack to fully
  // support recursion.
  for (const auto& Frame : this->DynamicFrameStack) {
    if (Frame.Function == StaticFunction) {
      // There is recursion some where.
      DEBUG(llvm::errs() << "Recursion detected for "
                         << StaticFunction->getName() << '\n');
      DEBUG(llvm::errs() << "Full call stack: ");
      for (const auto& Dump : this->DynamicFrameStack) {
        DEBUG(llvm::errs() << Dump.Function->getName() << " -> ");
      }
      DEBUG(llvm::errs() << StaticFunction->getName() << '\n');
    }
    assert(Frame.Function != StaticFunction && "Recursion detected!");
  }

  // ---
  // Start modification.
  // ---

  // Set the current function.
  this->CurrentBasicBlockName = "";
  this->CurrentIndex = -1;

  // Push the new frame.
  this->DynamicFrameStack.emplace_front(StaticFunction,
                                        std::move(DynamicArguments));
}

llvm::Instruction* DataGraph::getLLVMInstruction(
    const std::string& FunctionName, const std::string& BasicBlockName,
    const int Index) {
  assert(FunctionName == this->DynamicFrameStack.front().Function->getName() &&
         "Unmatched function name.");

  if (BasicBlockName != this->CurrentBasicBlockName) {
    // We are in a new basic block.
    // Find the new basic block.
    for (auto BBIter = this->DynamicFrameStack.front().Function->begin(),
              BBEnd = this->DynamicFrameStack.front().Function->end();
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

DynamicInstruction* DataGraph::getDynamicInstFromId(DynamicId Id) const {
  auto Iter = this->AliveDynamicInstsMap.find(Id);
  assert(Iter != this->AliveDynamicInstsMap.end() &&
         "Failed looking up id. Consider increasing window size?");
  return Iter->second;
}

const std::pair<std::string, uint64_t>&
DataGraph::getLatestMemBaseOffsetForStaticInstruction(
    llvm::Instruction* StaticInstruction) {
  auto Iter = this->StaticToLastMemBaseOffsetMap.find(StaticInstruction);
  if (Iter == this->StaticToLastMemBaseOffsetMap.end()) {
    DEBUG(llvm::errs() << "operand inst "
                       << StaticInstruction->getFunction()->getName()
                       << StaticInstruction->getParent()->getName()
                       << StaticInstruction->getName() << '\n');
  }
  assert((Iter != this->StaticToLastMemBaseOffsetMap.end()) &&
         "Static inst should have dynamic occurrence.");
  return Iter->second;
}

const std::string& DataGraph::getUniqueNameForStaticInstruction(
    llvm::Instruction* StaticInst) {
  // This function will be called a lot, cache the results.
  auto Iter = this->StaticInstructionUniqueNameMap.find(StaticInst);
  if (Iter == this->StaticInstructionUniqueNameMap.end()) {
    assert(StaticInst->getName() != "" &&
           "The static instruction should have a name to create a unique name "
           "for it.");
    auto TempTwine =
        StaticInst->getFunction()->getName() + "." + StaticInst->getName();
    Iter = this->StaticInstructionUniqueNameMap
               .emplace(std::piecewise_construct,
                        std::forward_as_tuple(StaticInst),
                        std::forward_as_tuple(TempTwine.str()))
               .first;
  }
  return Iter->second;
}

void DataGraph::handleMemoryBase(DynamicInstruction* DynamicInst) {
  llvm::Instruction* StaticInstruction = DynamicInst->getStaticInstruction();

  DEBUG(llvm::errs() << "handle memory base for inst "
                     << StaticInstruction->getFunction()->getName() << ' '
                     << StaticInstruction->getParent()->getName() << ' '
                     << StaticInstruction->getOpcodeName() << ' '
                     << StaticInstruction->getName() << '\n');

  assert(!llvm::isa<llvm::PHINode>(StaticInstruction) &&
         "handleMemoryBase called for phi node.");

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
      const auto& OperandMemBaseOffset =
          this->getLatestMemBaseOffsetForStaticInstruction(
              OperandStaticInstruction);
      DynamicInst->DynamicOperands[OperandIndex]->MemBase =
          OperandMemBaseOffset.first;
      DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
          OperandMemBaseOffset.second;
    } else if (auto OperandStaticArgument =
                   llvm::dyn_cast<llvm::Argument>(Operand)) {
      // This operand comes from an argument.
      // Look up into the DynamicFrameStack to get the value.
      const auto& DynamicFrame = this->DynamicFrameStack.front();
      auto DynamicValue = DynamicFrame.getArgValue(OperandStaticArgument);
      DynamicInst->DynamicOperands[OperandIndex]->MemBase =
          DynamicValue->MemBase;
      DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
          DynamicValue->MemOffset;
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
  }

  else if (auto AllocaStaticInstruction =
               llvm::dyn_cast<llvm::AllocaInst>(StaticInstruction)) {
    // Alloca is easy, set the base/offset to itself.
    DynamicInst->DynamicResult->MemBase =
        this->getUniqueNameForStaticInstruction(AllocaStaticInstruction);
    DynamicInst->DynamicResult->MemOffset = 0;
  }

  else if (auto BitCastStaticInstruction =
               llvm::dyn_cast<llvm::BitCastInst>(StaticInstruction)) {
    // Bit cast is simple so we support it here.
    DynamicValue* BaseValue = DynamicInst->DynamicOperands[0];
    DynamicInst->DynamicResult->MemBase = BaseValue->MemBase;
    DynamicInst->DynamicResult->MemOffset = BaseValue->MemOffset;
  }

  // To have some basic support of dynamic memory access.
  else if (auto LoadStaticInstruction =
               llvm::dyn_cast<llvm::LoadInst>(StaticInstruction)) {
    // We only worried about this if we are loading an pointer.
    if (LoadStaticInstruction->getType()->isPointerTy()) {
      // In that case, we init the base to itself, offset to 0.
      DynamicInst->DynamicResult->MemBase =
          this->getUniqueNameForStaticInstruction(LoadStaticInstruction);
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
    }
  }

  if (StaticInstruction->getName() != "") {
    // This inst will produce a result.
    this->StaticToLastMemBaseOffsetMap.emplace(
        std::piecewise_construct, std::forward_as_tuple(StaticInstruction),
        std::forward_as_tuple(DynamicInst->DynamicResult->MemBase,
                              DynamicInst->DynamicResult->MemOffset));
  }
  return;
}

void DataGraph::handleControlDependence(DynamicInstruction* DynamicInst) {
  auto DepInst = this->getPreviousBranchDynamicInstruction(DynamicInst);
  if (DepInst != nullptr) {
    // DEBUG(llvm::errs() << "Get control dependence!\n");
    if (this->CtrDeps.find(DynamicInst->Id) == this->CtrDeps.end()) {
      this->CtrDeps.emplace(DynamicInst->Id, std::unordered_set<DynamicId>());
    }
    this->CtrDeps.at(DynamicInst->Id).insert(DepInst->Id);
  }
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