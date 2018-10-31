
#ifndef LLVM_TDG_REPLAY_TRACE_H
#define LLVM_TDG_REPLAY_TRACE_H

#include "CacheWarmer.h"
#include "DataGraph.h"
#include "LocateAccelerableFunctions.h"
#include "LoopUtils.h"
#include "TDGSerializer.h"

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"

#include <map>
#include <string>

extern llvm::cl::opt<DataGraph::DataGraphDetailLv> DataGraphDetailLevel;
extern llvm::cl::opt<std::string> GemForgeOutputDataGraphFileName;
extern llvm::cl::opt<std::string> GemForgeOutputExtraFolderPath;

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
  virtual void dumpStats(std::ostream &O);

  /**
   * Compute all the static information and serialize in the tdg.
   */
  void computeStaticInfo();

  DataGraph *Trace;
  DataGraph::DataGraphDetailLv DGDetailLevel;

  std::string OutTraceName;
  std::string OutputExtraFolderPath;

  TDGSerializer *Serializer;
  CacheWarmer *CacheWarmerPtr;

  llvm::Module *Module;

  CachedLoopInfo *CachedLI;

  /**
   * Statistics.
   */
  uint64_t StatsDynamicInsts;

  /**
   * Static Informatin.
   */
  LLVM::TDG::StaticInformation StaticInfo;

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