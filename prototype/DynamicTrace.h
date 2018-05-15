#ifndef LLVM_TDG_DYNAMIC_TRACE_H
#define LLVM_TDG_DYNAMIC_TRACE_H

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DynamicValue {
 public:
  DynamicValue(const std::string& _Value);
  std::string Value;
  // Base/Offset of memory address.
  std::string MemBase;
  uint64_t MemOffset;
};

class DynamicInstruction {
 public:
  using DynamicId = uint64_t;

  DynamicInstruction(DynamicValue* _DynamicResult,
                     std::vector<DynamicValue*> _DynamicOperands,
                     DynamicInstruction* _Prev, DynamicInstruction* _Next);
  // Not copiable.
  DynamicInstruction(const DynamicInstruction& other) = delete;
  DynamicInstruction& operator=(const DynamicInstruction& other) = delete;
  // Not Movable.
  DynamicInstruction(DynamicInstruction&& other) = delete;
  DynamicInstruction& operator=(DynamicInstruction&& other) = delete;

  virtual ~DynamicInstruction();
  virtual const std::string& getOpName() = 0;
  virtual llvm::Instruction* getStaticInstruction() { return nullptr; }
  virtual void dump() {}

  DynamicValue* DynamicResult;

  DynamicInstruction* Prev;
  DynamicInstruction* Next;

  // This is important to store some constant/non-instruction generated
  // operands
  std::vector<DynamicValue*> DynamicOperands;
};

class LLVMDynamicInstruction : public DynamicInstruction {
 public:
  LLVMDynamicInstruction(llvm::Instruction* _StaticInstruction,
                         DynamicValue* _DynamicResult,
                         std::vector<DynamicValue*> _DynamicOperands,
                         DynamicInstruction* _Prev, DynamicInstruction* _Next);
  // Not copiable.
  LLVMDynamicInstruction(const LLVMDynamicInstruction& other) = delete;
  LLVMDynamicInstruction& operator=(const LLVMDynamicInstruction& other) =
      delete;
  // Not Movable.
  LLVMDynamicInstruction(LLVMDynamicInstruction&& other) = delete;
  LLVMDynamicInstruction& operator=(LLVMDynamicInstruction&& other) = delete;
  const std::string& getOpName() override { return this->OpName; }
  llvm::Instruction* getStaticInstruction() override {
    return this->StaticInstruction;
  }
  llvm::Instruction* StaticInstruction;
  std::string OpName;
};

class DynamicTrace {
 public:
  using DynamicId = DynamicInstruction::DynamicId;

  DynamicTrace(const std::string& _TraceFileName, llvm::Module* _Module);
  ~DynamicTrace();

  DynamicTrace(const DynamicTrace& other) = delete;
  DynamicTrace& operator=(const DynamicTrace& other) = delete;
  DynamicTrace(DynamicTrace&& other) = delete;
  DynamicTrace& operator=(DynamicTrace&& other) = delete;

  DynamicInstruction* DynamicInstructionListHead;
  DynamicInstruction* DynamicInstructionListTail;

  std::unordered_map<DynamicInstruction*,
                     std::unordered_set<DynamicInstruction*>>
      RegDeps;
  std::unordered_map<DynamicInstruction*,
                     std::unordered_set<DynamicInstruction*>>
      CtrDeps;
  std::unordered_map<DynamicInstruction*,
                     std::unordered_set<DynamicInstruction*>>
      MemDeps;

  // Map from llvm static instructions to dynamic instructions.
  // Ordered by the apperance of that dynamic instruction in the trace.
  std::unordered_map<llvm::Instruction*, std::list<DynamicInstruction*>>
      StaticToDynamicMap;

  llvm::Module* Module;

  llvm::DataLayout* DataLayout;

  // Some statistics.
  uint64_t NumMemDependences;

  static bool isBreakLine(const std::string& Line) {
    if (Line.size() == 0) {
      return true;
    }
    return Line[0] == 'i' || Line[0] == 'e';
  }

  /**
   * Split a string like a|b|c| into [a, b, c].
   */
  static std::vector<std::string> splitByChar(const std::string& source,
                                              char split) {
    std::vector<std::string> ret;
    size_t idx = 0, prev = 0;
    for (; idx < source.size(); ++idx) {
      if (source[idx] == split) {
        ret.push_back(source.substr(prev, idx - prev));
        prev = idx + 1;
      }
    }
    // Don't miss the possible last field.
    if (prev < idx) {
      ret.push_back(source.substr(prev, idx - prev));
    }
    return std::move(ret);
  }

 private:
  /**********************************************************************/
  /* These are temporary fields used in construnction only.
  /**********************************************************************/
  // Parse the dynamic instruction in the line buffer.
  void parseLineBuffer(const std::list<std::string>& LineBuffer);
  void parseDynamicInstruction(const std::list<std::string>& LineBuffer);
  void parseFunctionEnter(const std::list<std::string>& LineBuffer);

  std::string CurrentFunctionName;
  std::string CurrentBasicBlockName;
  int CurrentIndex;

  llvm::Function* CurrentFunction;
  llvm::BasicBlock* CurrentBasicBlock;
  llvm::BasicBlock::iterator CurrentStaticInstruction;

  llvm::Instruction* getLLVMInstruction(const std::string& FunctionName,
                                        const std::string& BasicBlockName,
                                        const int Index);

  DynamicInstruction* getLatestDynamicIdForStaticInstruction(
      llvm::Instruction* StaticInstruction);

  DynamicInstruction* getPreviousDynamicInstruction();

  DynamicInstruction* getPreviousNonPhiDynamicInstruction(
      DynamicInstruction* DynamicInst);
  DynamicInstruction* getPreviousBranchDynamicInstruction(
      DynamicInstruction* DynamicInst);

  // A map from virtual address to the last dynamic store instructions that
  // writes to it.
  std::unordered_map<uint64_t, DynamicInstruction*> AddrToLastStoreInstMap;
  // A map from virtual address to the last dynamic load instructions that
  // reads from it.
  std::unordered_map<uint64_t, DynamicInstruction*> AddrToLastLoadInstMap;

  // Handle the RAW, WAW, WAR dependence.
  void handleMemoryDependence(DynamicInstruction* DynamicInst);

  void handleControlDependence(DynamicInstruction* DynamicInst);

  // Return true if there is a dependence.
  bool checkAndAddMemoryDependence(
      std::unordered_map<uint64_t, DynamicInstruction*>& LastMap, uint64_t Addr,
      DynamicInstruction* DynamicInst);

  void handleRegisterDependence(DynamicInstruction* DynamicInst,
                                llvm::Instruction* StaticInstruction);
  void addRegisterDependence(DynamicInstruction* DynamicInst,
                             llvm::Instruction* OperandStaticInst);
  llvm::Instruction* resolveRegisterDependenceInPhiNode(
      llvm::Instruction* OperandStaticInst);

  // Records the information of the dynamic value inter-frame.
  // Especially used for mem/base initialization.
  std::list<std::unordered_map<llvm::Argument*, DynamicValue*>>
      DynamicFrameStack;

  // Handle the memory address base/offset.
  // Very important to be replayable in gem5.
  void handleMemoryBase(DynamicInstruction* DynamicInst);
};

#endif