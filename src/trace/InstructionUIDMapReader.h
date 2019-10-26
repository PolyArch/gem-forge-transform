#ifndef LLVM_TDG_INSTRUCTION_UID_MAP_READER_H
#define LLVM_TDG_INSTRUCTION_UID_MAP_READER_H

#include "UIDMap.pb.h"

#include <string>
#include <unordered_map>
#include <vector>

/**
 * Reading in the UIDMap.
 * We need this for the tracer, which doesn't have access to llvm.
 */
class InstructionUIDMapReader {
public:
  using InstructionUID = uint64_t;

  InstructionUIDMapReader() = default;
  InstructionUIDMapReader(const InstructionUIDMapReader &Other) = delete;
  InstructionUIDMapReader(InstructionUIDMapReader &&Other) = delete;
  InstructionUIDMapReader &
  operator=(const InstructionUIDMapReader &Other) = delete;
  InstructionUIDMapReader &operator=(InstructionUIDMapReader &&Other) = delete;

  void parseFrom(const std::string &FileName);

  const LLVM::TDG::InstructionDescriptor &
  getDescriptor(const InstructionUID UID) const;

private:
  LLVM::TDG::UIDMap Map;
};

#endif
