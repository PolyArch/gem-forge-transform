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

  DynamicInstruction(DynamicId _Id, llvm::Instruction* _StaticInstruction,
                     DynamicValue* _DynamicResult,
                     std::vector<DynamicValue*> _DynamicOperands);
  // Not copiable.
  DynamicInstruction(const DynamicInstruction& other) = delete;
  DynamicInstruction& operator=(const DynamicInstruction& other) = delete;
  // Not Movable.
  DynamicInstruction(DynamicInstruction&& other) = delete;
  DynamicInstruction& operator=(DynamicInstruction&& other) = delete;

  ~DynamicInstruction();

  DynamicId Id;
  llvm::Instruction* StaticInstruction;
  DynamicValue* DynamicResult;

  // This is important to store some constant/non-instruction generated
  // operands
  std::vector<DynamicValue*> DynamicOperands;
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

  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> RegDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> CtrDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> MemDeps;
  std::unordered_map<DynamicId, DynamicInstruction*> DyanmicInstsMap;

  // Map from llvm static instructions to dynamic instructions.
  // Ordered by the apperance of that dynamic instruction in the trace.
  std::unordered_map<llvm::Instruction*, std::list<DynamicInstruction*>>
      StaticToDynamicMap;

  llvm::Module* Module;

  llvm::DataLayout* DataLayout;

  // Some statistics.
  uint64_t NumMemDependences;

 private:
  /**********************************************************************/
  /* These are temporary fields used in construnction only.
  /**********************************************************************/
  // Parse the dynamic instruction in the line buffer.
  void parseLineBuffer(const std::list<std::string>& LineBuffer);
  void parseDynamicInstruction(const std::list<std::string>& LineBuffer);
  void parseFunctionEnter(const std::list<std::string>& LineBuffer);

  DynamicId CurrentDynamicId;

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

  DynamicInstruction* getPreviousNonPhiDynamicInstruction(DynamicId CurrentDynamicId);

  // A map from virtual address to the last dynamic store instructions that
  // writes to it.
  std::unordered_map<uint64_t, DynamicId> AddrToLastStoreInstMap;
  // A map from virtual address to the last dynamic load instructions that
  // reads from it.
  std::unordered_map<uint64_t, DynamicId> AddrToLastLoadInstMap;

  // Handle the RAW, WAW, WAR dependence.
  void handleMemoryDependence(DynamicInstruction* DynamicInst);

  // Return true if there is a dependence.
  bool checkAndAddMemoryDependence(
      std::unordered_map<uint64_t, DynamicId>& LastMap, uint64_t Addr,
      DynamicId CurrentDynamicId);

  void handleRegisterDependence(DynamicId CurrentDynamicId,
                                llvm::Instruction* StaticInstruction);
  void addRegisterDependence(DynamicId CurrentDynamicId,
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