
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
  ReplayTrace(char _ID = ID);
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
  llvm::Instruction* FakeRegisterSpill;
  llvm::Instruction* FakeRegisterFill;

  std::map<std::string, llvm::Constant*> GlobalStrings;

  // This map stores heuristic of the number of micro-ops a static instruction
  // can be translated into.
  std::unordered_map<llvm::Instruction*, uint32_t> FakeNumMicroOps;

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module& Module);

  void fakeRegisterAllocation();
  void fakeMicroOps();
  void fakeFixRegisterDeps();

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
  void formatOpCode(DynamicInstruction* DynamicInst, std::ofstream& Out);
};

#endif