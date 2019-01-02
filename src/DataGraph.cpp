#include "DataGraph.h"
#include "Utils.h"
#include "trace/TraceParserGZip.h"
#include "trace/TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

llvm::cl::opt<std::string> TraceFileName("trace-file",
                                         llvm::cl::desc("Trace file."));
llvm::cl::opt<std::string> TraceFileFormat(
    "trace-format", llvm::cl::desc("Trace file format."));

llvm::cl::opt<std::string> InstUIDFileName(
    "datagraph-inst-uid-file",
    llvm::cl::desc("Inst UID Map file for datagraph building."));

#define DEBUG_TYPE "DataGraph"

void AddrToMemAccessMap::update(Address Addr, DynamicId Id) {
  this->Log.emplace_back(Addr, Id);
  this->Map[Addr] = Id;
  this->release();
}

AddrToMemAccessMap::DynamicId AddrToMemAccessMap::getLastAccess(
    Address Addr) const {
  auto Iter = this->Map.find(Addr);
  if (Iter != this->Map.end()) {
    return Iter->second;
  }
  return DynamicInstruction::InvalidId;
}

void AddrToMemAccessMap::release() {
  while (this->Log.size() > AddrToMemAccessMap::LOG_THRESHOLD) {
    auto &Entry = this->Log.front();
    // Since we never erase entries in the map, this address should still be
    // there.
    auto Iter = this->Map.find(Entry.first);
    assert(Iter != this->Map.end() &&
           "This address has already been released.");
    if (Iter->second == Entry.second) {
      // This address has not been accessed by other instruction, it is
      // considered old and safe to remove it.
      this->Map.erase(Iter);
    }
    // Release the log.
    this->Log.pop_front();
  }
}

void MemoryDependenceComputer::update(Address Addr, size_t ByteSize,
                                      DynamicId Id, bool IsLoad) {
  auto &Map = IsLoad ? this->LoadMap : this->StoreMap;
  for (size_t I = 0; I < ByteSize; ++I) {
    Map.update(Addr + I, Id);
  }
}

void MemoryDependenceComputer::getMemoryDependence(
    Address Addr, size_t ByteSize, bool IsLoad,
    std::unordered_set<DynamicId> &OutMemDeps) const {
  for (size_t I = 0; I < ByteSize; ++I) {
    // R/WAW dependence.
    auto DepId = this->StoreMap.getLastAccess(Addr + I);
    if (DepId != DynamicInstruction::InvalidId) {
      OutMemDeps.insert(DepId);
    }
  }
  if (!IsLoad) {
    // WAR dependence.
    for (size_t I = 0; I < ByteSize; ++I) {
      auto DepId = this->LoadMap.getLastAccess(Addr + I);
      if (DepId != DynamicInstruction::InvalidId) {
        OutMemDeps.insert(DepId);
      }
    }
  }
}

DataGraph::DynamicFrame::DynamicFrame(
    llvm::Function *_Function,
    std::unordered_map<const llvm::Value *, DynamicValue> &&_Arguments)
    : Function(_Function),
      RunTimeEnv(std::move(_Arguments)),
      PrevBasicBlock(nullptr),
      PrevCallInst(nullptr) {
  // Initalize the register dependence look up table to empty list.
  for (auto BBIter = this->Function->begin(), BBEnd = this->Function->end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
         InstIter != InstEnd; ++InstIter) {
      this->RegDepLookUpMap.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(&*InstIter),
                                    std::forward_as_tuple());
    }
  }
}

DataGraph::DynamicFrame::~DynamicFrame() {
  // Nothing to release.
}

const DynamicValue &DataGraph::DynamicFrame::getValue(
    const llvm::Value *Value) const {
  const auto Iter = this->RunTimeEnv.find(Value);
  assert(Iter != this->RunTimeEnv.end() &&
         "Failed to find value in the frame.");
  return Iter->second;
}

const DynamicValue *DataGraph::DynamicFrame::getValueNullable(
    const llvm::Value *Value) const {
  const auto Iter = this->RunTimeEnv.find(Value);
  if (Iter == this->RunTimeEnv.end()) {
    return nullptr;
  } else {
    return &(Iter->second);
  }
}

void DataGraph::DynamicFrame::insertValue(const llvm::Value *Value,
                                          DynamicValue DValue) {
  DEBUG(llvm::errs() << "OFFSET " << DValue.MemOffset << '\n');
  // !! Emplace does not replace it! Erase it first!
  this->RunTimeEnv.erase(Value);
  this->RunTimeEnv.emplace(std::piecewise_construct,
                           std::forward_as_tuple(Value),
                           std::forward_as_tuple(std::move(DValue)));

  assert(this->getValue(Value).MemOffset == DValue.MemOffset &&
         "Invalid memory offset in run time env.");
}

const std::list<DataGraph::DynamicId>
    &DataGraph::DynamicFrame::translateRegisterDependence(
        llvm::Instruction *StaticInst) const {
  assert(StaticInst->getFunction() == this->Function &&
         "This instruction is not in our function.");
  return this->RegDepLookUpMap.at(StaticInst);
}

void DataGraph::DynamicFrame::updateRegisterDependenceLookUpMap(
    llvm::Instruction *StaticInst, DynamicId Id) {
  assert(StaticInst->getFunction() == this->Function &&
         "This instruction is not in our function.");
  auto Iter = this->RegDepLookUpMap.find(StaticInst);
  Iter->second.clear();
  Iter->second.push_back(Id);
}

void DataGraph::DynamicFrame::updateRegisterDependenceLookUpMap(
    llvm::Instruction *StaticInst, std::list<DynamicId> Ids) {
  assert(StaticInst->getFunction() == this->Function &&
         "This instruction is not in our function.");
  this->RegDepLookUpMap.at(StaticInst) = std::move(Ids);
}

DataGraph::DataGraph(llvm::Module *_Module, DataGraphDetailLv _DetailLevel)
    : Module(_Module),
      DataLayout(nullptr),
      DetailLevel(_DetailLevel),
      NumMemDependences(0),
      Parser(nullptr),
      PrevControlInstId(DynamicInstruction::InvalidId) {
  DEBUG(llvm::errs() << "Intializing datagraph.\n");
  assert(TraceFileName.getNumOccurrences() == 1 &&
         "Please specify the trace file.");

  if (TraceFileFormat.getNumOccurrences() == 0 ||
      TraceFileFormat.getValue() == "gzip") {
    this->Parser = new TraceParserGZip(TraceFileName);
  } else if (TraceFileFormat.getValue() == "protobuf") {
    DEBUG(llvm::errs() << "Creating parser.\n");
    if (InstUIDFileName.getNumOccurrences() > 0) {
      this->Parser =
          new TraceParserProtobuf(TraceFileName, InstUIDFileName.getValue());
    } else {
      this->Parser = new TraceParserProtobuf(TraceFileName);
    }
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

void DataGraph::commitDynamicInst(DynamicId Id) {
  auto DynamicInst = this->getAliveDynamicInst(Id);
  assert(DynamicInst != nullptr && "This dynamic inst is not alive.");
  // Remove all the dependence information.
  DEBUG(llvm::errs() << "Committing dynamic instruction " << Id
                     << " Op: " << DynamicInst->getOpName() << '\n');
  this->RegDeps.erase(Id);
  this->MemDeps.erase(Id);
  this->CtrDeps.erase(Id);
  this->AliveDynamicInstsMap.erase(Id);
  DEBUG(llvm::errs() << "Committing dynamic instruction "
                     << DynamicInst->getOpName() << '\n');
  delete DynamicInst;

#define DUMP_MEM_USAGE
#ifdef DUMP_MEM_USAGE
  static int Count = 0;
  Count++;
  if (Count % 100000 == 0) {
    Count = 0;
    this->printMemoryUsage();
  }
#endif
}

void DataGraph::commitOneDynamicInst() {
  assert(!this->DynamicInstructionList.empty() && "No inst to be committed.");
  // Remove all the dependence information.
  auto Id = this->DynamicInstructionList.front()->getId();
  this->commitDynamicInst(Id);
  this->DynamicInstructionList.pop_front();
}

DataGraph::DynamicInstIter DataGraph::insertDynamicInst(
    DynamicInstIter InsertBefore, DynamicInstruction *DynamicInst) {
  auto NewInstIter =
      this->DynamicInstructionList.emplace(InsertBefore, DynamicInst);

  this->RegDeps.emplace(std::piecewise_construct,
                        std::forward_as_tuple(DynamicInst->getId()),
                        std::forward_as_tuple());
  this->MemDeps.emplace(std::piecewise_construct,
                        std::forward_as_tuple(DynamicInst->getId()),
                        std::forward_as_tuple());
  this->CtrDeps.emplace(std::piecewise_construct,
                        std::forward_as_tuple(DynamicInst->getId()),
                        std::forward_as_tuple());

  this->AliveDynamicInstsMap.emplace(DynamicInst->getId(), NewInstIter);

  return NewInstIter;
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
      if (!Utils::isCallOrInvokeInst(StaticInstruction)) {
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
    // Also try to extract information from the operand.
    for (unsigned OperandIdx = 0,
                  NumOperands = StaticInstruction->getNumOperands();
         OperandIdx != NumOperands; ++OperandIdx) {
      auto OperandValue = StaticInstruction->getOperand(OperandIdx);
      if (llvm::isa<llvm::GlobalVariable>(OperandValue)) {
        // Insert to global variable env.
        if (this->GlobalValueEnv.count(OperandValue) == 0) {
          this->GlobalValueEnv.emplace(
              OperandValue, *(DynamicInst->DynamicOperands[OperandIdx]));
        } else {
          this->GlobalValueEnv.at(OperandValue) =
              *(DynamicInst->DynamicOperands[OperandIdx]);
        }
      }
      auto DynamicOperandValue =
          this->DynamicFrameStack.front().getValueNullable(OperandValue);
      if (DynamicOperandValue == nullptr) {
        this->DynamicFrameStack.front().insertValue(
            OperandValue, *(DynamicInst->DynamicOperands[OperandIdx]));
      }
    }
    if (StaticInstruction->getName() != "") {
      // This inst will produce a result.
      // Add the result to the tiny run time environment.
      if (DynamicInst->DynamicResult == nullptr) {
        // Somehow the dynamic result is missing.
        // The only legal case for this situation is for a traced call
        // instruction, whose result will come from the traced ret inst. We
        // explicitly check this case.
        assert(Utils::isCallOrInvokeInst(StaticInstruction) &&
               "Missing dynamic result for non-call inst.");
      } else {
        this->DynamicFrameStack.front().insertValue(
            StaticInstruction, *(DynamicInst->DynamicResult));
      }
    }
  }

  // Update the PrevControlInstId.
  this->updatePrevControlInstId(DynamicInst);

  // Add to the register dependence look up map.
  this->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
      StaticInstruction, DynamicInst->getId());

  // Update the previous block.
  this->DynamicFrameStack.front().setPrevBB(StaticInstruction->getParent());

  // If this is a ret, pop the stack.
  if (auto StaticRet = llvm::dyn_cast<llvm::ReturnInst>(StaticInstruction)) {
    this->DynamicFrameStack.pop_front();
    // We only care about this in Detail/Non-Simple Mode.
    if (this->DetailLevel > SIMPLE) {
      // If the ret has value, we should complete the memory base
      // and offset for the previous call inst.
      if (StaticRet->getReturnValue() != nullptr &&
          !this->DynamicFrameStack.empty()) {
        auto &Frame = this->DynamicFrameStack.front();
        // Check if there is a previous call instruction.
        if (Frame.PrevCallInst != nullptr) {
          // Update its run time value.
          Frame.insertValue(Frame.PrevCallInst,
                            *(DynamicInst->DynamicOperands[0]));
          // Clear the prev call inst.
          Frame.PrevCallInst = nullptr;
          Frame.CallStack.clear();
          assert(DynamicInst->DynamicOperands.size() == 1 &&
                 "There should be exactly one dynamic result for ret.");
        } else {
          // There is no previous call inst. This is possible when this a
          // partial datagraph, i.e. does not start with functionEnter.
          assert(this->DetailLevel < INTEGRATED &&
                 "Missing previous call inst in integrated mode.");
        }
      }
    }
  }

  // Set up the call stack if this is a call.
  if (Utils::isCallOrInvokeInst(StaticInstruction)) {
    // Set up the previous call instruction.
    this->DynamicFrameStack.front().PrevCallInst = StaticInstruction;
    auto &CallStack = this->DynamicFrameStack.front().CallStack;
    CallStack.clear();
    if (this->DetailLevel > SIMPLE) {
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
  this->AliveDynamicInstsMap[DynamicInst->getId()] = InsertedIter;

  return true;
}

void DataGraph::handleRegisterDependence(DynamicInstruction *DynamicInst,
                                         llvm::Instruction *StaticInstruction) {
  DEBUG(llvm::errs() << "Handling register dependence for ");
  DEBUG(this->printStaticInst(llvm::errs(), StaticInstruction));
  DEBUG(llvm::errs() << '\n');

  // Handle register dependence.
  assert(this->RegDeps.find(DynamicInst->getId()) == this->RegDeps.end() &&
         "This dynamic instruction's register dependence has already been "
         "handled.");

  assert(StaticInstruction->getOpcode() != llvm::Instruction::PHI &&
         "Can only handle register dependence for non phi node.");
  auto &RegDeps = this->RegDeps
                      .emplace(std::piecewise_construct,
                               std::forward_as_tuple(DynamicInst->getId()),
                               std::forward_as_tuple())
                      .first->second;

  for (unsigned int OperandId = 0,
                    NumOperands = StaticInstruction->getNumOperands();
       OperandId != NumOperands; ++OperandId) {
    if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
            StaticInstruction->getOperand(OperandId))) {
      // There is a register dependence on other instruction.
      for (auto DepId :
           this->DynamicFrameStack.front().translateRegisterDependence(
               OperandStaticInst)) {
        RegDeps.emplace_back(OperandStaticInst, DepId);
      }
    }
  }
}

void DataGraph::handlePhiNode(llvm::PHINode *StaticPhi,
                              const TraceParser::TracedInst &Parsed) {
  DEBUG(llvm::errs() << "Handle phi node " << Utils::formatLLVMInst(StaticPhi)
                     << '\n');
  // llvm::errs() << "Handle phi node " << Utils::formatLLVMInst(StaticPhi)
  //              << '\n';

  // First find the incoming value.
  llvm::Value *IncomingValue = nullptr;
  auto PrevBasicBlock = this->DynamicFrameStack.front().getPrevBB();

  if (PrevBasicBlock != nullptr) {
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
  } else {
    // There is no previous basic block. This can not happen in INTEGRATED
    // mode.
    assert(this->DetailLevel < INTEGRATED &&
           "Missing previous basic block in INTEGRATED mode.");
  }

  // Maintain the run time environment.
  // No need to worry about run time environment if we are in simple mode.
  if (this->DetailLevel > SIMPLE) {
    auto &Frame = this->DynamicFrameStack.front();
    if (IncomingValue != nullptr &&
        (llvm::isa<llvm::Instruction>(IncomingValue) ||
         llvm::isa<llvm::Argument>(IncomingValue))) {
      // For the incoming value of instruction and arguments, we look up the
      // run time env.
      // Make sure to copy it.
      const auto IncomingDynamicValue = Frame.getValueNullable(IncomingValue);
      if (IncomingDynamicValue != nullptr) {
        // Normal case, we found the incoming dynamic value.
        Frame.insertValue(StaticPhi, *IncomingDynamicValue);
      } else {
        // We failed to find the dynamic value. This can happen only when the
        // incoming value is a untraced-call instruction, or this is only a
        // partial datagraph.
        if (this->DetailLevel == INTEGRATED) {
          assert(Utils::isCallOrInvokeInst(IncomingValue) &&
                 "Non-call instruction missing dynamic value.");
        } else {
          // Simply ignore this missing value and insert ourselves.
        }
        // In such case, we have to use our own result from phi.
        Frame.insertValue(StaticPhi, DynamicValue(Parsed.Result));
      }
    } else {
      // We sliently create the default memory base/offset.
      Frame.insertValue(StaticPhi, DynamicValue(Parsed.Result));
    }
  }

  // Handle the dependence.
  {
    std::list<DynamicId> RegDeps;
    auto DepId = DynamicInstruction::InvalidId;
    if (IncomingValue != nullptr) {
      if (auto OperandStaticInst =
              llvm::dyn_cast<llvm::Instruction>(IncomingValue)) {
        // Loop for a dependent instruction.
        RegDeps = this->DynamicFrameStack.front().translateRegisterDependence(
            OperandStaticInst);
      }
    }
    // Update the register dependence look up map.
    this->DynamicFrameStack.front().updateRegisterDependenceLookUpMap(
        StaticPhi, std::move(RegDeps));
  }
}

void DataGraph::updateAddrToLastMemoryAccessMap(uint64_t Addr, size_t ByteSize,
                                                DynamicId Id, bool IsLoad) {
  this->MemDepsComputer.update(Addr, ByteSize, Id, IsLoad);
}

void DataGraph::handleMemoryDependence(DynamicInstruction *DynamicInst) {
  DEBUG(llvm::errs() << "Handling memory dependence for ");
  DEBUG(
      this->printStaticInst(llvm::errs(), DynamicInst->getStaticInstruction()));
  DEBUG(llvm::errs() << '\n');

  // Remember to create the dependence set before hand to ensure there is
  // always a set.
  auto &MemDep = this->MemDeps
                     .emplace(std::piecewise_construct,
                              std::forward_as_tuple(DynamicInst->getId()),
                              std::forward_as_tuple())
                     .first->second;

  if (auto LoadStaticInstruction =
          llvm::dyn_cast<llvm::LoadInst>(DynamicInst->getStaticInstruction())) {
    // Handle RAW dependence.
    assert(DynamicInst->DynamicOperands.size() == 1 &&
           "Invalid number of dynamic operands for load.");
    uint64_t BaseAddr = DynamicInst->DynamicOperands[0]->getAddr();
    llvm::Type *LoadedType =
        LoadStaticInstruction->getPointerOperandType()->getPointerElementType();
    uint64_t LoadedTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(LoadedType);

    this->MemDepsComputer.getMemoryDependence(BaseAddr, LoadedTypeSizeInByte,
                                              true /*true for load*/, MemDep);
    this->MemDepsComputer.update(BaseAddr, LoadedTypeSizeInByte,
                                 DynamicInst->getId(), true /*true for load*/);
    this->NumMemDependences += MemDep.size();
    return;
  }

  else if (auto StoreStaticInstruction = llvm::dyn_cast<llvm::StoreInst>(
               DynamicInst->getStaticInstruction())) {
    // Handle WAW and WAR dependence.
    assert(DynamicInst->DynamicOperands.size() == 2 &&
           "Invalid number of dynamic operands for store.");
    uint64_t BaseAddr = DynamicInst->DynamicOperands[1]->getAddr();
    llvm::Type *StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(StoredType);

    this->MemDepsComputer.getMemoryDependence(
        BaseAddr, StoredTypeSizeInByte, false /*false for store*/, MemDep);
    this->MemDepsComputer.update(BaseAddr, StoredTypeSizeInByte,
                                 DynamicInst->getId(),
                                 false /*false for store*/);
    this->NumMemDependences += MemDep.size();

    return;
  }
}

void DataGraph::parseFunctionEnter(TraceParser::TracedFuncEnter &Parsed) {
  llvm::Function *StaticFunction = this->Module->getFunction(Parsed.Func);
  assert(StaticFunction && "Failed to look up traced function in module.");

  // Handle the frame stack.
  std::unordered_map<const llvm::Value *, DynamicValue> DynamicArguments;

  // In simple mode, we don't care the actuall value parameter.
  if (this->DetailLevel > SIMPLE) {
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
        if (!CallStack.empty()) {
          // We found the call stack.
          assert(ArgumentIndex < CallStack.size() &&
                 "Invalid call stack, too small.");
          DynamicArgument.MemBase = CallStack[ArgumentIndex].MemBase;
          DynamicArgument.MemOffset = CallStack[ArgumentIndex].MemOffset;
        } else {
          // The call stack is missing.
          assert(this->DetailLevel < INTEGRATED &&
                 "Missing call stack in integrated mode.");
        }
      }

      ++ParsedArgumentIndex;
      ++ArgumentIter;
      ++ArgumentIndex;
    }
    assert(ArgumentIter == ArgumentEnd &&
           ParsedArgumentIndex == Parsed.Arguments.size() &&
           "Unmatched number of arguments and value lines in function enter.");
  }

  // ---
  // Start modification.
  // ---

  // Push the new frame.
  this->DynamicFrameStack.emplace_front(StaticFunction,
                                        std::move(DynamicArguments));
}

llvm::Instruction *DataGraph::getLLVMInstruction(
    const std::string &FunctionName, const std::string &BasicBlockName,
    const int Index) {
  llvm::Function *Func = this->Module->getFunction(FunctionName);
  assert(Func && "Failed to look up traced function in module.");

  auto FuncMapIter = this->MemorizedStaticInstMap.find(Func);
  if (FuncMapIter == this->MemorizedStaticInstMap.end()) {
    FuncMapIter =
        this->MemorizedStaticInstMap
            .emplace(std::piecewise_construct, std::forward_as_tuple(Func),
                     std::forward_as_tuple())
            .first;
  }

  auto &BBMap = FuncMapIter->second;
  auto BBMapIter = BBMap.find(BasicBlockName);
  if (BBMapIter == BBMap.end()) {
    // Iterate through function's Basic blocks to create all the bbs.
    for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
         ++BBIter) {
      std::string BBName = BBIter->getName().str();
      auto &InstVec =
          BBMap
              .emplace(std::piecewise_construct, std::forward_as_tuple(BBName),
                       std::forward_as_tuple())
              .first->second;
      // After this point BBName is moved.
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        InstVec.push_back(&*InstIter);
      }
    }
    BBMapIter = BBMap.find(BasicBlockName);
    assert(BBMapIter != BBMap.end() &&
           "Failed to find the basic block in BBMap.");
  }

  const auto &InstVec = BBMapIter->second;
  assert(Index >= 0 && Index < InstVec.size() && "Invalid Index.");
  auto Inst = InstVec[Index];

  // If this is a landing pad, we try to find the match function.
  // This is not 100% current, if an exception is thrown from a recursive
  // function.
  if (Inst->getOpcode() == llvm::Instruction::LandingPad ||
      Inst->getOpcode() == llvm::Instruction::CleanupPad) {
    while (this->DynamicFrameStack.empty() &&
           this->DynamicFrameStack.front().Function != Func) {
      this->DynamicFrameStack.pop_front();
    }
  }

  // If we are in integrated mode, sanity check to make sure that we are in
  // the correct frame.
  if (this->DetailLevel == INTEGRATED) {
    assert(!this->DynamicFrameStack.empty() &&
           "Empty frame stack for incoming instruction.");
    assert(this->DynamicFrameStack.front().Function == Func &&
           "Mismatch frame for incoming instruction.");
  } else {
    if (this->DynamicFrameStack.empty()) {
      // We should create an empty frame there.
      std::unordered_map<const llvm::Value *, DynamicValue> DynamicArguments;
      this->DynamicFrameStack.emplace_front(Func, std::move(DynamicArguments));
    }
    if (this->DynamicFrameStack.front().Function != Func) {
      DEBUG(llvm::errs() << "Mismatch frame "
                         << this->DynamicFrameStack.front().Function->getName()
                         << " with incoming instruction from "
                         << Func->getName() << '\n');
    }
    assert(this->DynamicFrameStack.front().Function == Func &&
           "Mismatch frame for incoming instruction.");
  }

  DEBUG(llvm::errs() << "getLLVMInstruction returns "
                     << Inst->getFunction()->getName() << " "
                     << Inst->getOpcodeName() << '\n');

  return Inst;
}

void DataGraph::updatePrevControlInstId(DynamicInstruction *DynamicInst) {
  switch (DynamicInst->getStaticInstruction()->getOpcode()) {
    case llvm::Instruction::Switch:
    case llvm::Instruction::Br:
    case llvm::Instruction::Ret:
    case llvm::Instruction::Call: {
      this->PrevControlInstId = DynamicInst->getId();
      break;
    }
    default:
      break;
  }
}

DataGraph::DynamicInstIter DataGraph::getDynamicInstFromId(DynamicId Id) const {
  auto Iter = this->AliveDynamicInstsMap.find(Id);
  assert(Iter != this->AliveDynamicInstsMap.end() &&
         "Failed looking up id. Consider increasing window size?");
  return Iter->second;
}

const std::string &DataGraph::getUniqueNameForStaticInstruction(
    llvm::Instruction *StaticInst) {
  // This function will be called a lot, cache the results.
  auto Iter = this->StaticInstructionUniqueNameMap.find(StaticInst);
  if (Iter == this->StaticInstructionUniqueNameMap.end()) {
    assert(StaticInst->getName() != "" &&
           "The static instruction should have a name to create a unique name "
           "for it.");
    auto TempTwine =
        StaticInst->getFunction()->getName() + "." + StaticInst->getName();
    // NOTE: I don't know why, but piecewise_construct here will result in
    // segment fault in O2 or higher mode, so I copy the string.
    Iter = this->StaticInstructionUniqueNameMap
               .emplace(StaticInst, TempTwine.str())
               .first;
  }
  return Iter->second;
}

void DataGraph::handleMemoryBase(DynamicInstruction *DynamicInst) {
  llvm::Instruction *StaticInstruction = DynamicInst->getStaticInstruction();

  DEBUG(llvm::errs() << "Handling memory base for ");
  DEBUG(this->printStaticInst(llvm::errs(), StaticInstruction));
  DEBUG(llvm::errs() << '\n');

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

      const auto DynamicValue = DynamicFrame.getValueNullable(Operand);
      if (DynamicValue != nullptr) {
        // We found the result.
        DynamicInst->DynamicOperands[OperandIndex]->MemBase =
            DynamicValue->MemBase;
        DynamicInst->DynamicOperands[OperandIndex]->MemOffset =
            DynamicValue->MemOffset;
      } else {
        // We cannot find the result in the run time env. The only possible
        // case is that it is an untraced call inst.
        assert(Utils::isCallOrInvokeInst(Operand) &&
               "Missing dynamic value for non-call instruction.");
      }

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

    uint64_t OperandAddr = BaseValue->getAddr();
    uint64_t ResultAddr = DynamicInst->DynamicResult->getAddr();
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
    const auto &UniqueId =
        this->getUniqueNameForStaticInstruction(AllocaStaticInstruction);
    DynamicInst->DynamicResult->MemBase = UniqueId;
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
    // Although store does not have result, it is good to assert that we have
    // a base here.
    assert(DynamicInst->DynamicOperands[1]->MemBase != "" &&
           "Failed to get memory base/offset for store inst.");
  }

  else if (auto SelectStaticInstruction =
               llvm::dyn_cast<llvm::SelectInst>(StaticInstruction)) {
    // Handle the select instruction.
    auto SelectedIndex =
        (DynamicInst->DynamicOperands[0]->getInt() == 1) ? 1 : 2;
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
    // result from callee's ret instruction. In that case, we have do defer
    // the processing until the callee returned.
    if (CallStaticInstruction->getName() != "") {
      if (DynamicInst->DynamicResult == nullptr) {
        // We don't have the result, hopefully this means that the callee is
        // traced. I do not like this early return here, but let's just keep
        // it like this so that we do not try to add to the run time env.
        return;
      } else {
        // We do have the result.
        // A special hack for now: if the callee is malloc, simply create a
        // new base to itself.
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
                            std::forward_as_tuple(DynamicInst->getId()),
                            std::forward_as_tuple())
                   .first->second;
  if (this->PrevControlInstId != DynamicInstruction::InvalidId) {
    Deps.insert(this->PrevControlInstId);
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

void DataGraph::printMemoryUsage() const {
  size_t StaticToLastDynamicMapSize = 0;
  for (const auto &Frame : this->DynamicFrameStack) {
    StaticToLastDynamicMapSize += Frame.getRegisterDependenceLookUpMapSize();
  }
  llvm::errs() << "====================================================\n";

  llvm::errs() << "DynamicInstructionList: "
               << this->DynamicInstructionList.size() << '\n';
  llvm::errs() << "AliveDynamicInstsMap:   "
               << this->AliveDynamicInstsMap.size() << '\n';
  llvm::errs() << "RegDeps:                " << this->RegDeps.size() << '\n';
  llvm::errs() << "CtrDeps:                " << this->CtrDeps.size() << '\n';
  llvm::errs() << "MemDeps:                " << this->MemDeps.size() << '\n';
  llvm::errs() << "StaticToLastDynamicMap: " << StaticToLastDynamicMapSize
               << '\n';
  llvm::errs() << "DynamicFrameStack:      " << this->DynamicFrameStack.size()
               << '\n';
  llvm::errs() << "AddrToLastStoreInstMap: "
               << this->MemDepsComputer.storeMapSize() << '\n';
  llvm::errs() << "AddrToLastLoadInstMap:  "
               << this->MemDepsComputer.loadMapSize() << '\n';

  llvm::errs() << "====================================================\n";
}

#undef DEBUG_TYPE
