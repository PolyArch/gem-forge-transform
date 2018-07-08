#include "DataGraph.h"
#include "trace/TraceParserGZip.h"
#include "trace/TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

static llvm::cl::opt<std::string> TraceFileName("trace-file",
                                                llvm::cl::desc("Trace file."));
static llvm::cl::opt<std::string>
    TraceFileFormat("trace-format", llvm::cl::desc("Trace file format."));

#define DEBUG_TYPE "DataGraph"

DataGraph::DynamicFrame::DynamicFrame(
    llvm::Function *_Function,
    std::unordered_map<llvm::Value *, DynamicValue> &&_Arguments)
    : Function(_Function), RunTimeEnv(std::move(_Arguments)),
      PrevBasicBlock(nullptr), PrevCallInst(nullptr),
      PrevControlInstId(DynamicInstruction::InvalidId) {}

DataGraph::DynamicFrame::~DynamicFrame() {
  // Nothing to release.
}

const DynamicValue &
DataGraph::DynamicFrame::getValue(llvm::Value *Value) const {
  const auto Iter = this->RunTimeEnv.find(Value);
  assert(Iter != this->RunTimeEnv.end() &&
         "Failed to find value in the frame.");
  return Iter->second;
}

void DataGraph::DynamicFrame::insertValue(llvm::Value *Value,
                                          DynamicValue DValue) {
  DEBUG(llvm::errs() << "OFFSET " << DValue.MemOffset << '\n');
  // !! Emplace does not replace it! Erase it first!
  this->RunTimeEnv.erase(Value);
  this->RunTimeEnv.emplace(std::piecewise_construct,
                           std::forward_as_tuple(Value),
                           std::forward_as_tuple(std::move(DValue)));

  if (this->getValue(Value).MemOffset != DValue.MemOffset) {
    const auto &Entry = this->getValue(Value);
    DEBUG(llvm ::errs() << "Unmatched memory offset current "
                        << DValue.MemOffset << " run time env " << Entry.Value
                        << ' ' << Entry.MemBase << ' ' << Entry.MemOffset
                        << '\n');
  }
  assert(this->getValue(Value).MemOffset == DValue.MemOffset &&
         "Invalid memory offset in run time env.");
}

void DataGraph::DynamicFrame::updatePrevControlInstId(
    DynamicInstruction *DInstruction) {
  switch (DInstruction->getStaticInstruction()->getOpcode()) {
  case llvm::Instruction::Switch:
  case llvm::Instruction::Br:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Call: {
    this->PrevControlInstId = DInstruction->Id;
    break;
  }
  default:
    break;
  }
}

DataGraph::DataGraph(llvm::Module *_Module, DataGraphDetailLv _DetailLevel)
    : Module(_Module), DataLayout(nullptr), DetailLevel(_DetailLevel),
      NumMemDependences(0), Parser(nullptr), CurrentBasicBlockName(""),
      CurrentIndex(-1) {

  DEBUG(llvm::errs() << "Intializing datagraph.\n");
  assert(TraceFileName.getNumOccurrences() == 1 &&
         "Please specify the trace file.");

  if (TraceFileFormat.getNumOccurrences() == 0 ||
      TraceFileFormat.getValue() == "gzip") {
    this->Parser = new TraceParserGZip(TraceFileName);
  } else if (TraceFileFormat.getValue() == "protobuf") {
    DEBUG(llvm::errs() << "Creating parser.\n");
    this->Parser = new TraceParserProtobuf(TraceFileName);
    DEBUG(llvm::errs() << "Creating parser. Done\n");
  } else {
    llvm_unreachable("Unknown trace file format.");
  }

  DEBUG(llvm::errs() << "Create data layout.\n");
  this->DataLayout = new llvm::DataLayout(this->Module);
}

DataGraph::~DataGraph() {
  // If there is any remaining inst, release them.
  while (!this->DynamicInstructionList.empty()) {
    delete this->DynamicInstructionList.front();
    this->DynamicInstructionList.pop_front();
  }
  delete this->DataLayout;
  DEBUG(llvm::errs() << "Releasing parser at " << this->Parser << '\n');
  delete this->Parser;
}

DataGraph::DynamicInstIter DataGraph::loadOneDynamicInst() {
  while (true) {
    auto NextType = this->Parser->getNextType();
    if (NextType == TraceParser::END) {
      return this->DynamicInstructionList.end();
    }
    switch (NextType) {
    case TraceParser::INST: {
      DEBUG(llvm::errs() << "Parse inst.\n");
      auto Parsed = this->Parser->parseLLVMInstruction();
      if (this->parseDynamicInstruction(Parsed)) {
        // We have found something new.
        // Return a iterator to the back element.
        return --(this->DynamicInstructionList.end());
      }
      break;
    }
    case TraceParser::FUNC_ENTER: {
      DEBUG(llvm::errs() << "Parse func enter.\n");
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
  return this->DynamicInstructionList.end();
}

void DataGraph::commitOneDynamicInst() {
  assert(!this->DynamicInstructionList.empty() && "No inst to be committed.");
  // Remove all the dependence information.
  auto Id = this->DynamicInstructionList.front()->Id;
  this->RegDeps.erase(Id);
  this->MemDeps.erase(Id);
  this->CtrDeps.erase(Id);
  this->AliveDynamicInstsMap.erase(Id);
  delete this->DynamicInstructionList.front();
  this->DynamicInstructionList.pop_front();
}

bool DataGraph::parseDynamicInstruction(TraceParser::TracedInst &Parsed) {

  DEBUG(llvm::errs() << "Parsed dynamic instruction.\n");

  llvm::Instruction *StaticInstruction =
      this->getLLVMInstruction(Parsed.Func, Parsed.BB, Parsed.Id);

  DEBUG(llvm::errs() << "Parsed dynamic instruction of ");
  DEBUG(this->printStaticInst(llvm::errs(), StaticInstruction));
  DEBUG(llvm::errs() << '\n');

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
    return false;
  }

  // Perform some sanity check if we are not in simple mode.
  if (this->DetailLevel > SIMPLE) {
    // Check the operands.
    assert(Parsed.Operands.size() == StaticInstruction->getNumOperands() &&
           "Unmatched # of dynamic value for operand.");

    assert(StaticInstruction != nullptr && "Non null static instruction ptr.");
    if (Parsed.Result != "") {
      // Sanity check to make sure that the static instruction do has result.
      if (StaticInstruction->getName() == "") {
        DEBUG(llvm::errs()
              << "DynamicResult set for non-result static instruction "
              << StaticInstruction->getName() << '\n');
      }
      assert(StaticInstruction->getName() != "" &&
             "DynamicResult set for non-result static instruction.");
    } else {
      // Comment this out as for some call inst, if the callee is traced,
      // then we donot log the result.
      if (!llvm::isa<llvm::CallInst>(StaticInstruction)) {
        assert(StaticInstruction->getName() == "" &&
               "Missing DynamicResult for non-call instruction.");
      }
    }
  }

  // Guarantee non phi node.
  // Create the new dynamic instructions.
  DynamicInstruction *DynamicInst =
      new LLVMDynamicInstruction(StaticInstruction, Parsed);

  this->handleRegisterDependence(DynamicInst, StaticInstruction);
  this->handleControlDependence(DynamicInst);

  // If in simple mode, we don't have the actual value, therefore
  // we shouldn't be worried about the memory dependence and memory base.
  if (this->DetailLevel > SIMPLE) {
    // Handle the dependence.
    this->handleMemoryDependence(DynamicInst);
    // Only try to handle memory base in integrated mode.
    if (this->DetailLevel > STANDALONE) {
      this->handleMemoryBase(DynamicInst);
    }
  }

  /***************************************************************************
   * Maintain the run time environment.
   * Insert the instruction into the list.
   * Update the PrevControlInstId.
   * Update the PrevBasicBlock.
   * If this is a ret, pop the frame stack
   *   -- Further more, if there is a call instruction above, update its base
   *      and offset.
   * If this is a call, set up CallStack of DynamicFrame.
   * Add to the list.
   * Add to the alive map.
   * Add to the static map.
   ***************************************************************************/

  // Maintain the run time environment.
  if (this->DetailLevel > SIMPLE) {
    if (StaticInstruction->getName() != "") {
      // This inst will produce a result.
      // Add the result to the tiny run time environment.
      if (DynamicInst->DynamicResult == nullptr) {
        // Somehow the dynamic result is missing.
        // The only legal case for this situation is for a traced call
        // instruction, whose result will come from the traced ret inst. We
        // explicitly check this case.
        assert(llvm::isa<llvm::CallInst>(StaticInstruction) &&
               "Missing dynamic result for non-call inst.");
      } else {
        this->DynamicFrameStack.front().insertValue(
            StaticInstruction, *(DynamicInst->DynamicResult));
      }
    }
  }

  // Update the PrevControlInstId.
  this->DynamicFrameStack.front().updatePrevControlInstId(DynamicInst);

  // Update the previous block.
  this->DynamicFrameStack.front().PrevBasicBlock =
      StaticInstruction->getParent();

  // If this is a ret, pop the stack.
  if (auto StaticRet = llvm::dyn_cast<llvm::ReturnInst>(StaticInstruction)) {
    this->DynamicFrameStack.pop_front();
    // Remember to clear the BB and Index.
    this->CurrentBasicBlock = nullptr;
    this->CurrentBasicBlockName = "";
    this->CurrentIndex = -1;
    // We only care about this in Detail/Non-Simple Mode.
    if (this->DetailLevel > SIMPLE) {
      // If the ret has value, we should complete the memory base
      // and offset for the previous call inst.
      if (StaticRet->getReturnValue() != nullptr &&
          !this->DynamicFrameStack.empty()) {
        // There should be a previous call instruction.
        auto &Frame = this->DynamicFrameStack.front();
        assert(Frame.PrevCallInst != nullptr &&
               "Failed to find paired PrevCallInst for ret.");
        assert(DynamicInst->DynamicOperands.size() == 1 &&
               "There should be exactly one dynamic result for ret.");
        // Update its run time value.
        Frame.insertValue(Frame.PrevCallInst,
                          *(DynamicInst->DynamicOperands[0]));
        // Clear the prev call inst.
        Frame.PrevCallInst = nullptr;
      }
    }
  }

  // Set up the call stack if this is a call.
  if (auto StaticCall = llvm::dyn_cast<llvm::CallInst>(StaticInstruction)) {
    auto &CallStack = this->DynamicFrameStack.front().CallStack;
    CallStack.clear();
    if (this->DetailLevel != SIMPLE) {
      // The last operand is the callee.
      for (size_t OperandIdx = 0;
           OperandIdx + 1 < DynamicInst->DynamicOperands.size(); ++OperandIdx) {
        CallStack.emplace_back(*(DynamicInst->DynamicOperands[OperandIdx]));
      }
    }
  }

  // Add to the list.
  auto InsertedIter = this->DynamicInstructionList.insert(
      this->DynamicInstructionList.end(), DynamicInst);

  // Add to the alive map.
  this->AliveDynamicInstsMap[DynamicInst->Id] = InsertedIter;

  // Add the map from static instrunction to dynamic.
  this->StaticToLastDynamicMap[StaticInstruction] = DynamicInst->Id;

  return true;
}

void DataGraph::handleRegisterDependence(DynamicInstruction *DynamicInst,
                                         llvm::Instruction *StaticInstruction) {
  DEBUG(llvm::errs() << "Handling register dependence for ");
  DEBUG(this->printStaticInst(llvm::errs(), StaticInstruction));
  DEBUG(llvm::errs() << '\n');

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

void DataGraph::handlePhiNode(llvm::PHINode *StaticPhi,
                              const TraceParser::TracedInst &Parsed) {

  DEBUG(llvm::errs() << "Handle phi node ";
        this->printStaticInst(llvm::errs(), StaticPhi); llvm::errs() << '\n');

  auto PrevBasicBlock = this->DynamicFrameStack.front().PrevBasicBlock;
  assert(PrevBasicBlock != nullptr && "Invalid previous basic block.");

  // First find the incoming value.
  llvm::Value *IncomingValue = nullptr;
  for (unsigned int OperandId = 0,
                    NumOperands = StaticPhi->getNumIncomingValues();
       OperandId != NumOperands; ++OperandId) {
    if (StaticPhi->getIncomingBlock(OperandId) == PrevBasicBlock) {
      IncomingValue = StaticPhi->getIncomingValue(OperandId);
      break;
    }
  }

  assert(IncomingValue != nullptr &&
         "Failed looking up the incoming value for phi node.");
  if (auto IncomingInst = llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
    DEBUG(llvm::errs() << "Incoming value is ";
          this->printStaticInst(llvm::errs(), IncomingInst);
          llvm::errs() << '\n');
  }

  // Maintain the run time environment.
  // No need to worry about run time environment if we are in simple mode.
  if (this->DetailLevel > SIMPLE) {
    auto &Frame = this->DynamicFrameStack.front();
    if (llvm::isa<llvm::Instruction>(IncomingValue) ||
        llvm::isa<llvm::Argument>(IncomingValue)) {
      // For the incoming value of instruction and arguments, we look up the
      // run time env.
      // Make sure to copy it.
      Frame.insertValue(StaticPhi, Frame.getValue(IncomingValue));
    } else {
      // We sliently create the default memory base/offset.
      Frame.RunTimeEnv.erase(StaticPhi);
      Frame.RunTimeEnv.emplace(std::piecewise_construct,
                               std::forward_as_tuple(StaticPhi),
                               std::forward_as_tuple(Parsed.Result));
    }
  }

  // Handle the dependence.
  {
    if (auto OperandStaticInst =
            llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
      // Two cases for incoming instruction
      // 1. If the incoming value is a phi node, look up the
      // PhiNodeDependenceMap.
      // 2. Otherwise, if it , look up the StaticToLastDynamicMap.
      DynamicId DependentId;
      bool Found = false;
      if (auto OperandStaticPhi =
              llvm::dyn_cast<llvm::PHINode>(IncomingValue)) {
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
    } else {
      // This is not produced by an instruction, we can silently ignore it.
    }
  }
}

bool DataGraph::checkAndAddMemoryDependence(
    std::unordered_map<uint64_t, DynamicId> &LastMap, uint64_t Addr,
    DynamicInstruction *DynamicInst) {
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

void DataGraph::updateAddrToLastMemoryAccessMap(uint64_t Addr, DynamicId Id,
                                                bool loadOrStore) {
  if (loadOrStore) {
    this->AddrToLastLoadInstMap[Addr] = Id;
  } else {
    this->AddrToLastStoreInstMap[Addr] = Id;
  }
}

void DataGraph::handleMemoryDependence(DynamicInstruction *DynamicInst) {

  DEBUG(llvm::errs() << "Handling memory dependence for ");
  DEBUG(
      this->printStaticInst(llvm::errs(), DynamicInst->getStaticInstruction()));
  DEBUG(llvm::errs() << '\n');

  // Remember to create the dependence set before hand to ensure there is
  // always a set.
  this->MemDeps.emplace(std::piecewise_construct,
                        std::forward_as_tuple(DynamicInst->Id),
                        std::forward_as_tuple());

  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->getStaticInstruction())) {

    // Handle RAW dependence.
    assert(DynamicInst->DynamicOperands.size() == 1 &&
           "Invalid number of dynamic operands for load.");
    uint64_t BaseAddr =
        std::stoull(DynamicInst->DynamicOperands[0]->Value, nullptr, 16);
    llvm::Type *LoadedType =
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
      this->AddrToLastLoadInstMap[Addr] = DynamicInst->Id;
    }
    return;
  }

  else if (auto StoreStaticInstruction = llvm::dyn_cast<llvm::StoreInst>(
               DynamicInst->getStaticInstruction())) {
    // Handle WAW and WAR dependence.
    assert(DynamicInst->DynamicOperands.size() == 2 &&
           "Invalid number of dynamic operands for store.");
    uint64_t BaseAddr =
        std::stoull(DynamicInst->DynamicOperands[1]->Value, nullptr, 16);
    llvm::Type *StoredType = StoreStaticInstruction->getPointerOperandType()
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
      this->AddrToLastStoreInstMap[Addr] = DynamicInst->Id;
    }
    return;
  }
}

void DataGraph::parseFunctionEnter(TraceParser::TracedFuncEnter &Parsed) {
  llvm::Function *StaticFunction = this->Module->getFunction(Parsed.Func);
  assert(StaticFunction && "Failed to look up traced function in module.");

  // Handle the frame stack.
  std::unordered_map<llvm::Value *, DynamicValue> DynamicArguments;

  // In simple mode, we don't care the actuall value parameter.
  if (this->DetailLevel != SIMPLE) {
    size_t ParsedArgumentIndex = 0;
    unsigned int ArgumentIndex = 0;
    auto ArgumentIter = StaticFunction->arg_begin();
    auto ArgumentEnd = StaticFunction->arg_end();
    while (ParsedArgumentIndex != Parsed.Arguments.size() &&
           ArgumentIter != ArgumentEnd) {
      llvm::Argument *StaticArgument = &*ArgumentIter;
      // Create the entry in the runtime env and get the reference.
      auto &DynamicArgument =
          DynamicArguments
              .emplace(
                  std::piecewise_construct,
                  std::forward_as_tuple(StaticArgument),
                  std::forward_as_tuple(Parsed.Arguments[ParsedArgumentIndex]))
              .first->second;

      // If there is a previous call inst, copy the base/offset.
      // Otherwise, initialize to itself.
      if (this->DynamicFrameStack.size() == 0) {
        // There is no caller.
        DynamicArgument.MemBase = StaticArgument->getName();
        DynamicArgument.MemOffset = 0;
      } else {
        // Get the dynamic value of calling stack.
        const auto &CallStack = this->DynamicFrameStack.front().CallStack;
        assert(ArgumentIndex < CallStack.size() &&
               "Invalid call stack, too small.");
        DynamicArgument.MemBase = CallStack[ArgumentIndex].MemBase;
        DynamicArgument.MemOffset = CallStack[ArgumentIndex].MemOffset;
        // // The previous inst must be a call.
        // DynamicInstruction *PrevDynamicInstruction =
        //     this->DynamicInstructionList.back();
        // assert(llvm::isa<llvm::CallInst>(
        //            PrevDynamicInstruction->getStaticInstruction()) &&
        //        "The previous instruction is not a call");
        // DynamicArgument.MemBase =
        //     PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemBase;
        // DynamicArgument.MemOffset =
        //     PrevDynamicInstruction->DynamicOperands[ArgumentIndex]->MemOffset;
      }

      ++ParsedArgumentIndex;
      ++ArgumentIter;
      ++ArgumentIndex;
    }
    assert(ArgumentIter == ArgumentEnd &&
           ParsedArgumentIndex == Parsed.Arguments.size() &&
           "Unmatched number of arguments and value lines in function enter.");
  }

  // Recursion check.
  // Currently our framework does not support recursion yet. I just make an
  // assertion here. It requires a more powerful dynamic frame stack to fully
  // support recursion.
  for (const auto &Frame : this->DynamicFrameStack) {
    if (Frame.Function == StaticFunction) {
      // There is recursion some where.
      DEBUG(llvm::errs() << "Recursion detected for "
                         << StaticFunction->getName() << '\n');
      DEBUG(llvm::errs() << "Full call stack: ");
      for (const auto &Dump : this->DynamicFrameStack) {
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

llvm::Instruction *
DataGraph::getLLVMInstruction(const std::string &FunctionName,
                              const std::string &BasicBlockName,
                              const int Index) {
  assert(!this->DynamicFrameStack.empty() &&
         "Empty frame stack when we get an incoming instruction.");
  assert(FunctionName == this->DynamicFrameStack.front().Function->getName() &&
         "Unmatched function name.");

  // DEBUG(llvm::errs() << "getLLVMInstruction picks function " << FunctionName
  //                    << '\n');

  if (BasicBlockName != this->CurrentBasicBlockName) {
    // We are in a new basic block.
    // Find the new basic block.
    for (auto BBIter = this->DynamicFrameStack.front().Function->begin(),
              BBEnd = this->DynamicFrameStack.front().Function->end();
         BBIter != BBEnd; ++BBIter) {
      // DEBUG(llvm::errs() << "getLLVMInstruction checking BB "
      //                    << BBIter->getName() << '\n');
      if (std::string(BBIter->getName()) == BasicBlockName) {
        this->CurrentBasicBlock = &*BBIter;
        this->CurrentBasicBlockName = BasicBlockName;
        // DEBUG(llvm::errs() << "Switch to basic block " << BasicBlockName
        //                    << '\n');
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

  llvm::Instruction *StaticInstruction = &*this->CurrentStaticInstruction;
  // Update the current index.
  this->CurrentIndex++;
  this->CurrentStaticInstruction++;
  return StaticInstruction;
}

DataGraph::DynamicInstIter DataGraph::getDynamicInstFromId(DynamicId Id) const {
  auto Iter = this->AliveDynamicInstsMap.find(Id);
  assert(Iter != this->AliveDynamicInstsMap.end() &&
         "Failed looking up id. Consider increasing window size?");
  return Iter->second;
}

const std::string &
DataGraph::getUniqueNameForStaticInstruction(llvm::Instruction *StaticInst) {
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

void DataGraph::handleMemoryBase(DynamicInstruction *DynamicInst) {
  llvm::Instruction *StaticInstruction = DynamicInst->getStaticInstruction();

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
    llvm::Value *Operand = StaticInstruction->getOperand(OperandIndex);
    if (llvm::isa<llvm::Instruction>(Operand) ||
        llvm::isa<llvm::Argument>(Operand)) {
      // This operand comes from an instruction.
      // Simply populates the result from the tiny run time env.
      const auto &DynamicFrame = this->DynamicFrameStack.front();
      const auto &DynamicValue = DynamicFrame.getValue(Operand);
      DynamicInst->DynamicOperands[OperandIndex]->MemBase =
          DynamicValue.MemBase;
      DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
          DynamicValue.MemOffset;

      DEBUG(llvm::errs()
            << "Handle memory base/offset for operand " << Operand->getName()
            << ' ' << DynamicInst->DynamicOperands[OperandIndex]->MemBase << ' '
            << DynamicInst->DynamicOperands[OperandIndex]->MemOffset << '\n');
    } else if (auto GlobalVar = llvm::dyn_cast<llvm::GlobalVariable>(Operand)) {
      // This is a global variable, we take a conservative approach here by
      // check it is an address.
      if (GlobalVar->getType()->isPointerTy()) {
        DynamicInst->DynamicOperands[OperandIndex]->MemBase =
            GlobalVar->getName();
        DynamicInst->DynamicOperands[OperandIndex]->MemOffset = 0;
      }
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
    DynamicValue *BaseValue = DynamicInst->DynamicOperands[0];

    assert(BaseValue->MemBase != "" && "Failed to get memory base for GEP.");

    DEBUG(llvm::errs() << "GEP " << BaseValue->Value << '\n');

    uint64_t OperandAddr = stoull(BaseValue->Value, nullptr, 16);
    uint64_t ResultAddr =
        stoull(DynamicInst->DynamicResult->Value, nullptr, 16);
    // Compute base/offset from the first operand.
    DynamicInst->DynamicResult->MemBase = BaseValue->MemBase;
    DynamicInst->DynamicResult->MemOffset =
        BaseValue->MemOffset + (ResultAddr - OperandAddr);

    {
      static int count = 0;
      if (true) {
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
    DynamicValue *BaseValue = DynamicInst->DynamicOperands[0];
    DynamicInst->DynamicResult->MemBase = BaseValue->MemBase;
    DynamicInst->DynamicResult->MemOffset = BaseValue->MemOffset;
  }

  // To have some basic support of dynamic memory access.
  else if (auto LoadStaticInstruction =
               llvm::dyn_cast<llvm::LoadInst>(StaticInstruction)) {
    // Assert the address.
    assert(DynamicInst->DynamicOperands[0]->MemBase != "" &&
           "Failed to get memory base/offset for load inst.");

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

  else if (auto StoreStaticInstruction =
               llvm::dyn_cast<llvm::StoreInst>(StaticInstruction)) {
    // Although store does not have result, it is good to assert that we have a
    // base here.
    assert(DynamicInst->DynamicOperands[1]->MemBase != "" &&
           "Failed to get memory base/offset for store inst.");
  }

  else if (auto SelectStaticInstruction =
               llvm::dyn_cast<llvm::SelectInst>(StaticInstruction)) {
    // Handle the select instruction.
    auto SelectedIndex =
        (stoull(DynamicInst->DynamicOperands[0]->Value) == 1) ? 1 : 2;
    DynamicInst->DynamicResult->MemBase =
        DynamicInst->DynamicOperands[SelectedIndex]->MemBase;
    DynamicInst->DynamicResult->MemOffset =
        DynamicInst->DynamicOperands[SelectedIndex]->MemOffset;
  }

  else if (auto CallStaticInstruction =
               llvm::dyn_cast<llvm::CallInst>(StaticInstruction)) {
    // Special case for the call instruction.
    // If the static call instruction has result, but missing from the dynamic
    // trace, it means that the callee is also traced and we should use the
    // result from callee's ret instruction. In that case, we have do defer the
    // processing until the callee returned.
    this->DynamicFrameStack.front().PrevCallInst = CallStaticInstruction;
    if (CallStaticInstruction->getName() != "") {
      if (DynamicInst->DynamicResult == nullptr) {
        // We don't have the result, hopefully this means that the callee is
        // traced. I do not like this early return here, but let's just keep it
        // like this so that we do not try to add to the run time env.
        return;
      } else {
        // We do have the result.
        // A special hack for now: if the callee is malloc, simply create a new
        // base to itself.
        auto Function = CallStaticInstruction->getCalledFunction();
        if (Function != nullptr && Function->getName() == "malloc") {
          DynamicInst->DynamicResult->MemBase =
              this->getUniqueNameForStaticInstruction(StaticInstruction);
          DynamicInst->DynamicResult->MemOffset = 0;
        }
      }
    }
  }

  return;
}

void DataGraph::handleControlDependence(DynamicInstruction *DynamicInst) {
  // Ensure the empty set property.
  auto &Deps = this->CtrDeps
                   .emplace(std::piecewise_construct,
                            std::forward_as_tuple(DynamicInst->Id),
                            std::forward_as_tuple())
                   .first->second;
  if (this->DynamicFrameStack.front().PrevControlInstId !=
      DynamicInstruction::InvalidId) {
    Deps.insert(this->DynamicFrameStack.front().PrevControlInstId);
  }
}

DynamicInstruction *DataGraph::getAliveDynamicInst(DynamicId Id) {
  auto AliveMapIter = this->AliveDynamicInstsMap.find(Id);
  if (AliveMapIter == this->AliveDynamicInstsMap.end()) {
    return nullptr;
  }
  return *(AliveMapIter->second);
}

void DataGraph::printStaticInst(llvm::raw_ostream &O,
                                llvm::Instruction *StaticInst) const {
  O << StaticInst->getParent()->getParent()->getName()
    << "::" << StaticInst->getParent()->getName()
    << "::" << StaticInst->getName() << '(' << StaticInst->getOpcodeName()
    << ')';
}

#undef DEBUG_TYPE