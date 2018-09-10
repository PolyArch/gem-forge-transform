#ifndef LLVM_TDG_INSTRUCTION_UID_MAP_H
#define LLVM_TDG_INSTRUCTION_UID_MAP_H

#include "llvm/IR/Instruction.h"

#include <string>
#include <unordered_map>

/**
 * Maintains a simple uid for each instruction.
 * For now just use the instruction's address.
 */
class InstructionUIDMap {
public:
  struct InstructionDescriptor {
    std::string FuncName;
    std::string BBName;
    int PosInBB;
    InstructionDescriptor(std::string _FuncName, std::string _BBName,
                          int _PosInBB)
        : FuncName(std::move(_FuncName)), BBName(std::move(_BBName)),
          PosInBB(_PosInBB) {}
  };

  using InstructionUID = uint64_t;

  InstructionUIDMap() = default;
  InstructionUIDMap(const InstructionUIDMap &Other) = delete;
  InstructionUIDMap(InstructionUIDMap &&Other) = delete;
  InstructionUIDMap &operator=(const InstructionUIDMap &Other) = delete;
  InstructionUIDMap &operator=(InstructionUIDMap &&Other) = delete;

  InstructionUID getOrAllocateUID(llvm::Instruction *Inst, int PosInBB = -1);
  void serializeTo(const std::string &FileName) const;
  void parseFrom(const std::string &FileName);

  const InstructionDescriptor &getDescriptor(const InstructionUID UID) const;

private:
  std::unordered_map<InstructionUID, InstructionDescriptor> Map;
};

#endif