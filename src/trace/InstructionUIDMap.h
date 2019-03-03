#ifndef LLVM_TDG_INSTRUCTION_UID_MAP_H
#define LLVM_TDG_INSTRUCTION_UID_MAP_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Type.h"

#include <string>
#include <unordered_map>
#include <vector>

/**
 * Maintains a simple uid for each instruction.
 * For now just use the instruction's address.
 */
class InstructionUIDMap {
 public:
  struct InstructionValueDescriptor {
    bool IsParam;
    llvm::Type::TypeID TypeID;

    InstructionValueDescriptor(bool _IsParam, llvm::Type::TypeID _TypeID)
        : IsParam(_IsParam), TypeID(_TypeID) {}
  };

  struct InstructionDescriptor {
    std::string OpName;
    std::string FuncName;
    std::string BBName;
    int PosInBB;
    std::vector<InstructionValueDescriptor> Values;
    InstructionDescriptor(std::string _OpName, std::string _FuncName,
                          std::string _BBName, int _PosInBB,
                          std::vector<InstructionValueDescriptor> _Values)
        : OpName(std::move(_OpName)),
          FuncName(std::move(_FuncName)),
          BBName(std::move(_BBName)),
          PosInBB(_PosInBB),
          Values(std::move(_Values)) {}
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