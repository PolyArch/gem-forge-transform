#include "trace/InstructionUIDMapReader.h"

#include <cassert>
#include <fstream>

void InstructionUIDMapReader::parseFrom(const std::string &FileName) {
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

const InstructionUIDMapReader::InstructionDescriptor &
InstructionUIDMapReader::getDescriptor(const InstructionUID UID) const {
  auto Iter = this->Map.find(UID);
  assert(Iter != this->Map.end() && "Unrecognized UID.");
  return Iter->second;
}