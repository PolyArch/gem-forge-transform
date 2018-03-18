#include "DynamicTrace.h"

#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <vector>

#define DEBUG_TYPE "DynamicTrace"

namespace {
bool isBreakLine(const std::string& Line) {
  if (Line.size() == 0) {
    return true;
  }
  return Line[0] == 'i' || Line[0] == 'e';
}

/**
 * Split a string like a|b|c| into [a, b, c].
 */
std::vector<std::string> splitByChar(const std::string& source, char split) {
  std::vector<std::string> ret;
  size_t idx = 0, prev = 0;
  for (; idx < source.size(); ++idx) {
    if (source[idx] == split) {
      ret.push_back(source.substr(prev, idx - prev));
      prev = idx + 1;
    }
  }
  // Don't miss the possible last field.
  if (prev < idx) {
    ret.push_back(source.substr(prev, idx - prev));
  }
  return std::move(ret);
}
}  // namespace

DynamicValue::DynamicValue(const std::string& _Value)
    : Value(_Value), MemBase(""), MemOffset(0) {}

DynamicInstruction::DynamicInstruction(
    DynamicId _Id, llvm::Instruction* _StaticInstruction,
    DynamicValue* _DynamicResult, std::vector<DynamicValue*> _DynamicOperands)
    : Id(_Id),
      StaticInstruction(_StaticInstruction),
      DynamicResult(_DynamicResult),
      DynamicOperands(std::move(_DynamicOperands)) {
  assert(this->StaticInstruction != nullptr &&
         "Non null static instruction ptr.");
  if (this->DynamicResult != nullptr) {
    // Sanity check to make sure that the static instruction do has result.
    assert(this->StaticInstruction->getName() != "" &&
           "DynamicResult set for non-result static instruction.");
  } else {
    assert(this->StaticInstruction->getName() == "" &&
           "Missing DynamicResult for result static instruction.");
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
    : Module(_Module),
      NumMemDependences(0),
      CurrentFunctionName(""),
      CurrentBasicBlockName(""),
      CurrentIndex(-1) {
  this->DataLayout = new llvm::DataLayout(this->Module);

  // Parse the trace file.
  std::ifstream TraceFile(_TraceFileName);
  assert(TraceFile.is_open() && "Failed to open trace file.");
  std::string Line;
  std::list<std::string> LineBuffer;
  while (std::getline(TraceFile, Line)) {
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
  TraceFile.close();
}

DynamicTrace::~DynamicTrace() {
  for (auto& Iter : this->DyanmicInstsMap) {
    delete Iter.second;
    Iter.second = nullptr;
  }
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

  static DynamicId CurrentDynamicId = 0;
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
        auto ResultLineFields = splitByChar(*LineIter, '|');
        DynamicResult = new DynamicValue(ResultLineFields[4]);
        break;
      }
      case 'p': {
        assert(OperandIndex < StaticInstruction->getNumOperands() &&
               "Too much operands.");
        auto OperandLineFields = splitByChar(*LineIter, '|');
        if (OperandLineFields.size() < 5) {
          DEBUG(llvm::errs() << *LineIter << '\n');
        }
        assert(OperandLineFields.size() >= 5 &&
               "Too few fields for operand line.");
        DynamicOperands[OperandIndex++] =
            new DynamicValue(OperandLineFields[4]);
        break;
      }
      default: {
        llvm_unreachable("Unknown value line.");
        break;
      }
    }
    ++LineIter;
  }

  // Create the new dynamic instructions.
  DynamicInstruction* DynamicInst =
      new DynamicInstruction(CurrentDynamicId, StaticInstruction, DynamicResult,
                             std::move(DynamicOperands));

  this->handleRegisterDependence(CurrentDynamicId, StaticInstruction);
  this->handleMemoryDependence(DynamicInst);

  // Add to the map.
  this->DyanmicInstsMap.emplace(CurrentDynamicId, DynamicInst);
  // Add the map from static instrunction to dynamic.
  if (this->StaticToDynamicMap.find(StaticInstruction) ==
      this->StaticToDynamicMap.end()) {
    this->StaticToDynamicMap.emplace(StaticInstruction,
                                     std::list<DynamicInstruction*>());
  }
  this->StaticToDynamicMap.at(StaticInstruction).push_back(DynamicInst);
  CurrentDynamicId++;
}

void DynamicTrace::addRegisterDependence(DynamicId CurrentDynamicId,
                                         llvm::Instruction* OperandStaticInst) {
  // Push the latest dynamic inst of the operand static inst.
  if (this->StaticToDynamicMap.find(OperandStaticInst) ==
      this->StaticToDynamicMap.end()) {
    DEBUG(llvm::errs() << "operand inst "
                       << OperandStaticInst->getFunction()->getName()
                       << OperandStaticInst->getParent()->getName()
                       << OperandStaticInst->getName() << '\n');
  }
  assert(this->StaticToDynamicMap.find(OperandStaticInst) !=
             this->StaticToDynamicMap.end() &&
         "Operand static inst should have dynamic occurrence.");
  const auto& OperandDynamicInsts =
      this->StaticToDynamicMap.at(OperandStaticInst);
  assert(OperandDynamicInsts.size() != 0 &&
         "Operand static inst should have at least one dynamic occurrence.");
  this->RegDeps[CurrentDynamicId].insert(OperandDynamicInsts.back()->Id);
}

void DynamicTrace::handleRegisterDependence(
    DynamicId CurrentDynamicId, llvm::Instruction* StaticInstruction) {
  // Handle register dependence.
  assert(this->RegDeps.find(CurrentDynamicId) == this->RegDeps.end() &&
         "Used DynamicId.");
  this->RegDeps.emplace(CurrentDynamicId, std::unordered_set<DynamicId>());

  // Special rule for phi node.
  if (auto PhiStaticInst = llvm::dyn_cast<llvm::PHINode>(StaticInstruction)) {
    // Get the previous basic block.
    assert(this->DyanmicInstsMap.find(CurrentDynamicId - 1) !=
               this->DyanmicInstsMap.end() &&
           "There should be an inst before phi inst.");
    llvm::BasicBlock* PrevBasicBlock =
        this->DyanmicInstsMap.at(CurrentDynamicId - 1)
            ->StaticInstruction->getParent();
    // Check all the incoming values.
    for (unsigned int OperandId = 0,
                      NumOperands = PhiStaticInst->getNumIncomingValues();
         OperandId != NumOperands; ++OperandId) {
      if (PhiStaticInst->getIncomingBlock(OperandId) == PrevBasicBlock) {
        // We found the previous block.
        // Check if the value is produced by an inst.
        if (auto OperandStaticInst = llvm::dyn_cast<llvm::Instruction>(
                PhiStaticInst->getIncomingValue(OperandId))) {
          this->addRegisterDependence(CurrentDynamicId, OperandStaticInst);
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
      this->addRegisterDependence(CurrentDynamicId, OperandStaticInst);
    }
  }
}

bool DynamicTrace::checkAndAddMemoryDependence(
    std::unordered_map<uint64_t, DynamicId>& LastMap, uint64_t Addr,
    DynamicId CurrentDynamicId) {
  auto Iter = LastMap.find(Addr);
  if (Iter != LastMap.end()) {
    // There is a dependence.
    auto DepIter = this->MemDeps.find(CurrentDynamicId);
    if (DepIter == this->MemDeps.end()) {
      DepIter = this->MemDeps
                    .emplace(CurrentDynamicId, std::unordered_set<DynamicId>())
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
    uint64_t BaseAddr = std::stoull(DynamicInst->DynamicOperands[0]->Value);
    llvm::Type* LoadedType =
        LoadStaticInstruction->getPointerOperandType()->getPointerElementType();
    uint64_t LoadedTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(LoadedType);
    for (uint64_t ByteIter = 0; ByteIter < LoadedTypeSizeInByte; ++ByteIter) {
      uint64_t Addr = BaseAddr + ByteIter;
      if (this->checkAndAddMemoryDependence(this->AddrToLastStoreInstMap, Addr,
                                            DynamicInst->Id)) {
        this->NumMemDependences++;
      }
      // Update the latest read map.
      this->AddrToLastLoadInstMap.emplace(Addr, DynamicInst->Id);
    }
    return;
  }

  if (auto StoreStaticInstruction =
          llvm::dyn_cast<llvm::StoreInst>(DynamicInst->StaticInstruction)) {
    // Handle WAW and WAR dependence.
    assert(DynamicInst->DynamicOperands.size() == 2 &&
           "Invalid number of dynamic operands for store.");
    uint64_t BaseAddr = std::stoull(DynamicInst->DynamicOperands[1]->Value);
    llvm::Type* StoredType = StoreStaticInstruction->getPointerOperandType()
                                 ->getPointerElementType();
    uint64_t StoredTypeSizeInByte =
        this->DataLayout->getTypeStoreSize(StoredType);
    for (uint64_t ByteIter = 0; ByteIter < StoredTypeSizeInByte; ++ByteIter) {
      uint64_t Addr = BaseAddr + ByteIter;
      // WAW.
      if (this->checkAndAddMemoryDependence(this->AddrToLastStoreInstMap, Addr,
                                            DynamicInst->Id)) {
        this->NumMemDependences++;
      }
      // WAR.
      if (this->checkAndAddMemoryDependence(this->AddrToLastLoadInstMap, Addr,
                                            DynamicInst->Id)) {
        this->NumMemDependences++;
      }
      // Update the last store map.
      this->AddrToLastStoreInstMap.emplace(Addr, DynamicInst->Id);
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

  // Set the current function.
  this->CurrentFunctionName = EnterLineFields[1];
  this->CurrentFunction = this->Module->getFunction(this->CurrentFunctionName);
  assert(this->CurrentFunction &&
         "Failed to loop up traced function in module.");
  // Clear basic block and Index.
  this->CurrentBasicBlockName = "";
  this->CurrentIndex = -1;
}

llvm::Instruction* DynamicTrace::getLLVMInstruction(
    const std::string& FunctionName, const std::string& BasicBlockName,
    const int Index) {
  if (FunctionName != this->CurrentFunctionName) {
    this->CurrentFunctionName = FunctionName;
    this->CurrentFunction =
        this->Module->getFunction(this->CurrentFunctionName);
    assert(this->CurrentFunction &&
           "Failed to loop up traced function in module.");
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

#undef DEBUG_TYPE