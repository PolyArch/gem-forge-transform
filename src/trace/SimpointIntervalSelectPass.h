#ifndef LLVM_TDG_SIMPOINT_INTERVAL_SELECT_PASS_H
#define LLVM_TDG_SIMPOINT_INTERVAL_SELECT_PASS_H

#include "../GemForgeBasePass.h"
#include "CallLoopProfileTree.h"

class SimpointIntervalSelectPass : public GemForgeBasePass {
public:
  static char ID;
  SimpointIntervalSelectPass();
  ~SimpointIntervalSelectPass() override;

  bool runOnModule(llvm::Module &Module) override;

private:
  CallLoopProfileTree *CLProfileTree = nullptr;

  llvm::FunctionCallee EdgeMarkFunc;

  void generateProfileInterval();
  void generateEdgeTimeline();
  void registerEdgeMarkFunc();
  void insertEdgeMark(const CallLoopProfileTree::EdgePtr &Edge);
};

#endif