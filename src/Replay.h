
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

class ReplayTrace : public llvm::FunctionPass {
public:
  using DynamicId = DataGraph::DynamicId;
  static char ID;
  ReplayTrace(char _ID = ID);
  virtual ~ReplayTrace();

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;

  bool doInitialization(llvm::Module &Module) override;

  bool runOnFunction(llvm::Function &Function) override;

  bool doFinalization(llvm::Module &Module) override;

protected:
  virtual void transform();

  DataGraph *Trace;

  std::string OutTraceName;

  TDGSerializer *Serializer;

  llvm::Module *Module;

  /**
   * The analysis can only be used in run* methods, so we will
   * use Transformed guard and call transform in runOnFunction
   * only once.
   */
  bool Transformed;

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