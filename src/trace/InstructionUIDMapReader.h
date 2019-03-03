#ifndef LLVM_TDG_INSTRUCTION_UID_MAP_READER_H
#define LLVM_TDG_INSTRUCTION_UID_MAP_READER_H

#include <string>
#include <unordered_map>
#include <vector>

/**
 * Reading in the UIDMap.
 * Maintains a simple uid for each instruction.
 * For now just use the instruction's address.
 */
class InstructionUIDMapReader {
 public:
  struct InstructionValueDescriptor {
    bool IsParam;
    unsigned TypeID;

    InstructionValueDescriptor(bool _IsParam, unsigned _TypeID)
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

  InstructionUIDMapReader() = default;
  InstructionUIDMapReader(const InstructionUIDMapReader &Other) = delete;
  InstructionUIDMapReader(InstructionUIDMapReader &&Other) = delete;
  InstructionUIDMapReader &operator=(const InstructionUIDMapReader &Other) =
      delete;
  InstructionUIDMapReader &operator=(InstructionUIDMapReader &&Other) = delete;

  void parseFrom(const std::string &FileName);

  const InstructionDescriptor &getDescriptor(const InstructionUID UID) const;

 private:
  std::unordered_map<InstructionUID, InstructionDescriptor> Map;
};

#endif