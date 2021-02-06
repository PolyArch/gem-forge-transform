#ifndef LLVM_TDG_STREAM_PASS_OPTIONS_H
#define LLVM_TDG_STREAM_PASS_OPTIONS_H

#include "llvm/Support/CommandLine.h"

#include "StreamStrategy.h"

extern llvm::cl::opt<bool> StreamPassAllowAliasedStream;

extern llvm::cl::opt<bool> StreamPassEnableStore;
extern llvm::cl::opt<bool> StreamPassEnableConditionalStep;

extern llvm::cl::opt<bool> StreamPassEnableReduce;

extern llvm::cl::opt<StreamPassChooseStrategyE> StreamPassChooseStrategy;

extern llvm::cl::opt<std::string> StreamWhitelistFileName;

extern llvm::cl::opt<bool> StreamPassUpgradeLoadToUpdate;
extern llvm::cl::opt<bool> StreamPassMergePredicatedStore;
extern llvm::cl::opt<bool> StreamPassMergeIndPredicatedStore;
extern llvm::cl::opt<bool> StreamPassEnableValueDG;
extern llvm::cl::opt<bool> StreamPassEnableNestStream;

#endif