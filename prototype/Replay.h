
#ifndef LLVM_TDG_REPLAY_TRACE_H
#define LLVM_TDG_REPLAY_TRACE_H

#include "DynamicTrace.h"
#include "LocateAccelerableFunctions.h"

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include <map>
#include <string>

class ReplayTrace : public llvm::FunctionPass {
 public:
  using DynamicId = DynamicTrace::DynamicId;
  static char ID;
  ReplayTrace();
  virtual ~ReplayTrace();

  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override;

  bool doInitialization(llvm::Module& Module) override;

  bool runOnFunction(llvm::Function& Function) override;

 protected:
  virtual void TransformTrace();

  DynamicTrace* Trace;

  std::string OutTraceName;

  llvm::Module* Module;

  llvm::Value* ReplayFunc;

  std::map<std::string, llvm::Constant*> GlobalStrings;

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module& Module);

  //************************************************************************//
  // Helper function to generate the trace for gem5.
  //************************************************************************//
  void formatInstruction(DynamicInstruction* DynamicInst, std::ofstream& Out);
  void formatDeps(DynamicInstruction* DynamicInst, std::ofstream& Out);
  void formatOpCode(llvm::Instruction* StaticInstruction, std::ofstream& Out);
};

#endif