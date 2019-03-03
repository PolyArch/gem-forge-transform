#ifndef LLVM_TDG_INSTRUCTION_UID_MAP_READER_H
#define LLVM_TDG_INSTRUCTION_UID_MAP_READER_H

#include <string>
#include <unordered_map>

/**
 * Reading in the UIDMap.
 * Maintains a simple uid for each instruction.
 * For now just use the instruction's address.
 */
class InstructionUIDMapReader {
 public:
  struct InstructionDescriptor {
    std::string OpName;
    std::string FuncName;
    std::string BBName;
    int PosInBB;
    InstructionDescriptor(std::string _OpName, std::string _FuncName,
                          std::string _BBName, int _PosInBB)
        : OpName(std::move(_OpName)),
          FuncName(std::move(_FuncName)),
          BBName(std::move(_BBName)),
          PosInBB(_PosInBB) {}
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