#ifndef LLVM_TDG_IDENTICAL_TRANSFORMER_H
#define LLVM_TDG_IDENTICAL_TRANSFORMER_H

#include "DynamicTrace.h"

class IdenticalTransformer {
 public:
  IdenticalTransformer(DynamicTrace* _Trace);

  IdenticalTransformer(const IdenticalTransformer& other) = delete;
  IdenticalTransformer& operator=(const IdenticalTransformer& other) = delete;

  IdenticalTransformer(IdenticalTransformer&& other) = delete;
  IdenticalTransformer& operator=(const IdenticalTransformer& other) = delete;

  DynamicTrace* Trace;
 private:
  
};

#endif