
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

  // This map stores heuristic of the number of micro-ops a static instruction
  // can be translated into.
  std::unordered_map<llvm::Instruction*, uint32_t> FakeNumMicroOps;

  uint32_t estimateNumMicroOps(llvm::Instruction* StaticInstruction);

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module& Module);

  void fakeRegisterAllocation();

  //************************************************************************//
  // Helper function to generate the trace for gem5.
  //************************************************************************//
  void formatInstruction(
      DynamicInstruction* DynamicInst, std::ofstream& Out,
      const std::unordered_map<DynamicInstruction*, DynamicId>&
          AllocatedDynamicIdMap);
  void formatDeps(DynamicInstruction* DynamicInst, std::ofstream& Out,
                  const std::unordered_map<DynamicInstruction*, DynamicId>&
                      AllocatedDynamicIdMap);
  void formatOpCode(llvm::Instruction* StaticInstruction, std::ofstream& Out);
};

#endif