#include "trace/InstructionUIDMapReader.h"

#include <cassert>
#include <fstream>

void InstructionUIDMapReader::parseFrom(const std::string &FileName) {
  assert(this->Map.inst_map().empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");
  this->Map.ParseFromIstream(&I);
  I.close();
}

const LLVM::TDG::InstructionDescriptor &
InstructionUIDMapReader::getDescriptor(const InstructionUID UID) const {
  auto Iter = this->Map.inst_map().find(UID);
  assert(Iter != this->Map.inst_map().end() && "Unrecognized UID.");
  return Iter->second;
}
