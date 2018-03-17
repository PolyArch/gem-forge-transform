#ifndef LLVM_TDG_DYNAMIC_TRACE_H
#define LLVM_TDG_DYNAMIC_TRACE_H

#include "llvm/IR/Module.h"

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DynamicValue {
 public:
  DynamicValue() {}
  std::string Value;
  // Base/Offset of memory address.
  std::string MemBase;
  uint64_t MemOffset;
};

class DynamicInstruction {
 public:
  using DynamicId = uint64_t;

  DynamicInstruction(DynamicId _Id, llvm::Instruction* _StaticInstruction);

  DynamicId Id;
  llvm::Instruction* StaticInstruction;
  std::list<DynamicValue*> DynamicValues;
};

class DynamicTrace {
 public:
  using DynamicId = DynamicInstruction::DynamicId;

  DynamicTrace(const std::string& _TraceFileName, llvm::Module* _Module);
  ~DynamicTrace();

  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> RegDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> CtrDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> MemDeps;
  std::unordered_map<DynamicId, DynamicInstruction*> DyanmicInstsMap;

  // Map from llvm static instructions to dynamic instructions.
  // Ordered by the apperance of that dynamic instruction in the trace.
  std::unordered_map<llvm::Instruction*, std::list<DynamicInstruction*>>
      StaticToDynamicMap;

  llvm::Module* Module;

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
};

#endif