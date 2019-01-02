#include "trace/InstructionUIDMap.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

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

    // Return value of str is rvalue, will be moved.
    this->Map.emplace(
        std::piecewise_construct, std::forward_as_tuple(UID),
        std::forward_as_tuple(std::string(Inst->getOpcodeName()),
                              Inst->getFunction()->getName().str(),
                              BB->getName().str(), PosInBB));
  }
  return UID;
}

void InstructionUIDMap::serializeTo(const std::string &FileName) const {
  std::ofstream O(FileName);
  assert(O.is_open() && "Failed to open InstructionUIDMap serialization file.");
  for (const auto &Record : this->Map) {
    O << Record.first << ' ' << Record.second.FuncName << ' '
      << Record.second.BBName << ' ' << Record.second.PosInBB << ' '
      << Record.second.OpName << '\n';
  }
}

void InstructionUIDMap::parseFrom(const std::string &FileName) {
  assert(this->Map.empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");
  InstructionUID UID;
  std::string FuncName, BBName, OpName;
  int PosInBB;
  while (I >> UID >> FuncName >> BBName >> PosInBB >> OpName) {
    // Local variables will be copied.
    auto Inserted =
        this->Map
            .emplace(std::piecewise_construct, std::forward_as_tuple(UID),
                     std::forward_as_tuple(OpName, FuncName, BBName, PosInBB))
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