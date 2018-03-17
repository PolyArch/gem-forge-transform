#include "DynamicTrace.h"

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

DynamicInstruction::DynamicInstruction(DynamicId _Id,
                                       llvm::Instruction* _StaticInstruction)
    : Id(_Id), StaticInstruction(_StaticInstruction) {}

DynamicTrace::DynamicTrace(const std::string& _TraceFileName,
                           llvm::Module* _Module)
    : Module(_Module),
      CurrentFunctionName(""),
      CurrentBasicBlockName(""),
      CurrentIndex(-1) {
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
  // TODO: Parse the dynamic values.

  // Create the new dynamic instructions.
  DynamicInstruction* DynamicInst =
      new DynamicInstruction(CurrentDynamicId, StaticInstruction);
  // Add to the map.
  this->DyanmicInstsMap.emplace(CurrentDynamicId, DynamicInst);
  // Add the map from static instrunction to dynamic.
  if (this->StaticToDynamicMap.find(StaticInstruction) == this->StaticToDynamicMap.end()) {
    this->StaticToDynamicMap.emplace(StaticInstruction, std::list<DynamicInstruction*>());
  }
  this->StaticToDynamicMap.at(StaticInstruction).push_back(DynamicInst);
  CurrentDynamicId++;
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