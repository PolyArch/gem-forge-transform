#include "trace/InstructionUIDMap.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include <fstream>

InstructionUIDMap::InstructionUID InstructionUIDMap::getOrAllocateUID(
    llvm::Instruction *Inst, int PosInBB) {
  auto UID = reinterpret_cast<InstructionUID>(Inst);
  if (this->Map.count(UID) == 0) {
    // New instruction.
    auto BB = Inst->getParent();
    if (PosInBB == -1) {
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        PosInBB++;
        if (Inst == (&*InstIter)) {
          break;
        }
      }
    }
    assert(PosInBB >= 0 && "Negative PosInBB.");

    // Create the value vector.
    std::vector<InstructionValueDescriptor> Values;

    if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(Inst)) {
      for (unsigned IncomingValueId = 0,
                    NumIncomingValues = PHIInst->getNumIncomingValues();
           IncomingValueId < NumIncomingValues; IncomingValueId++) {
        auto IncomingValue = PHIInst->getIncomingValue(IncomingValueId);
        auto IncomingBlock = PHIInst->getIncomingBlock(IncomingValueId);
        bool IsParam = true;
        auto TypeID = IncomingBlock->getType()->getTypeID();
        Values.emplace_back(IsParam, TypeID);
      }
    } else {
      for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
           OperandId < NumOperands; OperandId++) {
        auto Operand = Inst->getOperand(OperandId);
        bool IsParam = true;
        auto TypeID = Operand->getType()->getTypeID();

        Values.emplace_back(IsParam, TypeID);
      }
    }

    if (Inst->getName() != "") {
      bool IsParam = false;
      auto TypeID = Inst->getType()->getTypeID();

      Values.emplace_back(IsParam, TypeID);
    }

    // Return value of str is rvalue, will be moved.
    this->Map.emplace(
        std::piecewise_construct, std::forward_as_tuple(UID),
        std::forward_as_tuple(std::string(Inst->getOpcodeName()),
                              Inst->getFunction()->getName().str(),
                              BB->getName().str(), PosInBB, std::move(Values)));
  }
  return UID;
}

void InstructionUIDMap::serializeTo(const std::string &FileName) const {
  std::ofstream O(FileName);
  assert(O.is_open() && "Failed to open InstructionUIDMap serialization file.");
  for (const auto &Record : this->Map) {
    O << Record.first << ' ' << Record.second.FuncName << ' '
      << Record.second.BBName << ' ' << Record.second.PosInBB << ' '
      << Record.second.OpName << ' ' << Record.second.Values.size();
    for (const auto &Value : Record.second.Values) {
      O << ' ' << Value.IsParam << ' ' << Value.TypeID;
    }
    O << '\n';
  }
}

void InstructionUIDMap::parseFrom(const std::string &FileName) {
  assert(this->Map.empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");
  InstructionUID UID;
  std::string FuncName, BBName, OpName;
  int PosInBB, NumValues;

  bool IsParam;
  unsigned TypeID;

  while (I >> UID >> FuncName >> BBName >> PosInBB >> OpName >> NumValues) {
    std::vector<InstructionValueDescriptor> Values;
    Values.reserve(NumValues);
    for (int Idx = 0; Idx < NumValues; ++Idx) {
      I >> IsParam >> TypeID;
      Values.emplace_back(IsParam, static_cast<llvm::Type::TypeID>(TypeID));
    }

    // Local variables will be copied.
    auto Inserted =
        this->Map
            .emplace(std::piecewise_construct, std::forward_as_tuple(UID),
                     std::forward_as_tuple(OpName, FuncName, BBName, PosInBB,
                                           Values))
            .second;
    assert(Inserted && "Failed to insert the record.");
  }
}

const InstructionUIDMap::InstructionDescriptor &
InstructionUIDMap::getDescriptor(const InstructionUID UID) const {
  auto Iter = this->Map.find(UID);
  assert(Iter != this->Map.end() && "Unrecognized UID.");
  return Iter->second;
}