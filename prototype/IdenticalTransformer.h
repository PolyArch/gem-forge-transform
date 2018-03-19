#ifndef LLVM_TDG_IDENTICAL_TRANSFORMER_H
#define LLVM_TDG_IDENTICAL_TRANSFORMER_H

#include "DynamicTrace.h"

class IdenticalTransformer {
 public:
  IdenticalTransformer(DynamicTrace* _Trace, const std::string& _OutTraceName);

  IdenticalTransformer(const IdenticalTransformer& other) = delete;
  IdenticalTransformer& operator=(const IdenticalTransformer& other) = delete;

  IdenticalTransformer(IdenticalTransformer&& other) = delete;
  IdenticalTransformer& operator=(IdenticalTransformer&& other) = delete;

  DynamicTrace* Trace;

  using DynamicId = DynamicTrace::DynamicId;

 private:
  void formatInstruction(DynamicInstruction* DynamicInst, std::ofstream& Out);
  void formatOpCode(llvm::Instruction* StaticInstruction, std::ofstream& Out);
  void formatDeps(DynamicInstruction* DynamicInst, std::ofstream& Out);
};

#endif