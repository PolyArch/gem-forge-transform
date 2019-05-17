
#ifndef LLVM_TDG_DYNAMIC_INSTRUCTION_H
#define LLVM_TDG_DYNAMIC_INSTRUCTION_H

#include "TDGInstruction.pb.h"
#include "trace/TraceParser.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <fstream>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declaration.
class DataGraph;

class DynamicValue {
public:
  DynamicValue(const std::string &_Value, const std::string &_MemBase = "",
               uint64_t _MemOffset = 0);
  DynamicValue(const DynamicValue &Other);
  DynamicValue &operator=(const DynamicValue &Other);
  explicit DynamicValue(DynamicValue &&Other);
  // Serialize to bytes according to the type.
  std::string serializeToBytes(llvm::Type *Type) const;
  // Interpret the value as an address.
  uint64_t getAddr() const {
    return *(reinterpret_cast<const uint64_t *>(this->Value.data()));
  }
  // Interpret the value as an int.
  uint64_t getInt() const { return this->getAddr(); }
  double getDouble() const {
    return *(reinterpret_cast<const double *>(this->Value.data()));
  }
  float getFloat() const { return this->getDouble(); }
  /**
   * ! The DynamicValue::Value size is not the real size of the loaded value.
   * ! For example, for int32_t, I always store 8 bytes here for simplicity.
   */
  std::string Value;
  // Base/Offset of memory address.
  std::string MemBase;
  uint64_t MemOffset;
};

class DynamicInstruction {
public:
  using DynamicId = uint64_t;
  using DependentMap =
      std::unordered_map<DynamicId, std::unordered_set<DynamicId>>;

  DynamicInstruction();
  // Not copiable.
  DynamicInstruction(const DynamicInstruction &other) = delete;
  DynamicInstruction &operator=(const DynamicInstruction &other) = delete;
  // Not Movable.
  DynamicInstruction(DynamicInstruction &&other) = delete;
  DynamicInstruction &operator=(DynamicInstruction &&other) = delete;

  virtual ~DynamicInstruction();
  virtual std::string getOpName() const = 0;
  virtual llvm::Instruction *getStaticInstruction() const { return nullptr; }
  virtual void dump() {}

  DynamicId getId() const { return this->Id; }

  static DynamicId InvalidId;

  DynamicValue *DynamicResult;

  // This is important to store some constant/non-instruction generated
  // operands
  std::vector<DynamicValue *> DynamicOperands;

  void addUsedStreamId(uint64_t UsedStreamId);

  void format(llvm::raw_ostream &Out, DataGraph *DG) const;

  void serializeToProtobuf(LLVM::TDG::TDGInstruction *ProtobufEntry,
                           DataGraph *DG) const;

protected:
  /**
   * Special constructor to allow take in dynamic id.
   */
  DynamicInstruction(DynamicId _Id);
  /**
   * 0 IS RESERVED FOR INVALID DYNAMIC ID.
   */
  static DynamicId allocateId();

  DynamicId Id;

  /**
   * Used by stream isa.
   */
  std::unordered_set<uint64_t> *UsedStreamIds;

  void formatDeps(llvm::raw_ostream &Out, DataGraph *DG) const;
  void formatOpCode(llvm::raw_ostream &Out) const;

  virtual void formatCustomizedFields(llvm::raw_ostream &Out,
                                      DataGraph *Trace) const {}

  virtual void
  serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                           DataGraph *DG) const {}
};

class LLVMDynamicInstruction : public DynamicInstruction {
public:
  LLVMDynamicInstruction(llvm::Instruction *_StaticInstruction,
                         TraceParser::TracedInst &_Parsed);
  LLVMDynamicInstruction(llvm::Instruction *_StaticInstruction,
                         DynamicValue *_Result,
                         std::vector<DynamicValue *> _Operands);
  // Not copiable.
  LLVMDynamicInstruction(const LLVMDynamicInstruction &other) = delete;
  LLVMDynamicInstruction &
  operator=(const LLVMDynamicInstruction &other) = delete;
  // Not Movable.
  LLVMDynamicInstruction(LLVMDynamicInstruction &&other) = delete;
  LLVMDynamicInstruction &operator=(LLVMDynamicInstruction &&other) = delete;
  std::string getOpName() const override;
  llvm::Instruction *getStaticInstruction() const override {
    return this->StaticInstruction;
  }
  llvm::Instruction *StaticInstruction;
  std::string OpName;

  void formatCustomizedFields(llvm::raw_ostream &Out,
                              DataGraph *Trace) const override;

  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override;

private:
  /**
   * Try to find the dynamically what is the next static instruction.
   * Should only be used for branches.
   *
   * For br: we determine the next static instruction from the dynamic value.
   * Otherwise: we look at the next dynamic instruction from DG.
   *
   * Returns nullptr if failed.
   */
  const llvm::Instruction *getDynamicNextStaticInst(DataGraph *DG) const;
};

#endif