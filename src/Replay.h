
#ifndef LLVM_TDG_REPLAY_TRACE_H
#define LLVM_TDG_REPLAY_TRACE_H

#include "DataGraph.h"
#include "LocateAccelerableFunctions.h"
#include "TDGSerializer.h"

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include <map>
#include <string>

extern llvm::cl::opt<DataGraph::DataGraphDetailLv> DataGraphDetailLevel;

class ReplayTrace : public llvm::ModulePass {
public:
  using DynamicId = DataGraph::DynamicId;
  static char ID;
  ReplayTrace(char _ID = ID);
  virtual ~ReplayTrace();

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;

  bool runOnModule(llvm::Module &Module) override;

  bool processFunction(llvm::Function &Function);

protected:
  virtual bool initialize(llvm::Module &Module);
  virtual void transform();
  virtual bool finalize(llvm::Module &Module);

  DataGraph *Trace;

  std::string OutTraceName;

  TDGSerializer *Serializer;

  llvm::Module *Module;

  llvm::Value *ReplayFunc;
  llvm::Instruction *FakeRegisterSpill;
  llvm::Instruction *FakeRegisterFill;

  std::map<std::string, llvm::Constant *> GlobalStrings;

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module &Module);

  void fakeRegisterAllocation();
  void fakeFixRegisterDeps();
  /*************************************************************************
   * Local transformations, which only require the local information.
   */
  void fakeMicroOps();
  void fakeExternalCall(DataGraph::DynamicInstIter DynamicInstIter);
};

#endif