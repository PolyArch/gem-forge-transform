#include "trace/InstructionUIDMapReader.h"

#include <cassert>
#include <fstream>

void InstructionUIDMapReader::parseFrom(const std::string &FileName) {
  assert(this->Map.empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");
  // Parse the inst uid.
  size_t InstUIDSize;
  assert((I >> InstUIDSize) && "Failed to read in the number of inst uids.");
  for (size_t InstUIDLine = 0; InstUIDLine < InstUIDSize; ++InstUIDLine) {

    InstructionUID UID;
    std::string FuncName, BBName, OpName;
    int PosInBB, NumValues;

    bool IsParam;
    unsigned TypeID;

    assert((I >> UID >> FuncName >> BBName >> PosInBB >> OpName >> NumValues) &&
           "Failed to read in the inst uid.");
    std::vector<InstructionValueDescriptor> Values;
    Values.reserve(NumValues);
    for (int Idx = 0; Idx < NumValues; ++Idx) {
      I >> IsParam >> TypeID;
      Values.emplace_back(IsParam, TypeID);
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
  // Ignore the FuncUID.
}

const InstructionUIDMapReader::InstructionDescriptor &
InstructionUIDMapReader::getDescriptor(const InstructionUID UID) const {
  auto Iter = this->Map.find(UID);
  assert(Iter != this->Map.end() && "Unrecognized UID.");
  return Iter->second;
}