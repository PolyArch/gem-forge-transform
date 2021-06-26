#include "StreamPassOptions.h"

llvm::cl::opt<bool> StreamPassAllowAliasedStream(
    "stream-pass-allow-aliased-stream",
    llvm::cl::desc("Allow aliased streams to be specialized."));

llvm::cl::opt<bool>
    StreamPassEnableStore("stream-pass-enable-store", llvm::cl::init(false),
                          llvm::cl::desc("Enable store stream."));

llvm::cl::opt<bool> StreamPassEnableConditionalStep(
    "stream-pass-enable-conditional-step", llvm::cl::init(false),
    llvm::cl::desc("Enable conditional step stream."));

llvm::cl::opt<bool>
    StreamPassEnableReduce("stream-pass-enable-reduce", llvm::cl::init(false),
                           llvm::cl::desc("Enable reduce stream."));

llvm::cl::opt<StreamPassChooseStrategyE> StreamPassChooseStrategy(
    "stream-pass-choose-strategy",
    llvm::cl::desc("Choose how to choose the configure loop level:"),
    llvm::cl::values(clEnumValN(StreamPassChooseStrategyE::DYNAMIC_OUTER_MOST,
                                "dynamic-outer",
                                "Always pick the outer most loop level."),
                     clEnumValN(StreamPassChooseStrategyE::STATIC_OUTER_MOST,
                                "static-outer",
                                "Always pick the outer most loop level."),
                     clEnumValN(StreamPassChooseStrategyE::INNER_MOST, "inner",
                                "Always pick the inner most loop level.")));

llvm::cl::opt<std::string> StreamWhitelistFileName(
    "stream-whitelist-file",
    llvm::cl::desc("Stream whitelist instruction file."));

llvm::cl::opt<bool> StreamPassUpgradeLoadToUpdate(
    "stream-pass-upgrade-load-to-update", llvm::cl::init(false),
    llvm::cl::desc("Upgrade load stream to update stream."));

llvm::cl::opt<bool> StreamPassMergePredicatedStore(
    "stream-pass-merge-predicated-store", llvm::cl::init(false),
    llvm::cl::desc("Merge predicated store stream into load stream."));

llvm::cl::opt<bool> StreamPassMergeIndPredicatedStore(
    "stream-pass-merge-ind-predicated-store", llvm::cl::init(false),
    llvm::cl::desc("Merge predicated store stream into indirect load stream."));

llvm::cl::opt<bool> StreamPassEnableValueDG(
    "stream-pass-enable-value-dg", llvm::cl::init(false),
    llvm::cl::desc("Analyze ValueDG for store/atomic streams."));

llvm::cl::opt<bool> StreamPassEnableFuseLoadOp(
    "stream-pass-enable-fuse-load-op", llvm::cl::init(false),
    llvm::cl::desc("Fuse profitable load ops into streams."));

llvm::cl::opt<bool> StreamPassEnableNestStream(
    "stream-pass-enable-nest-stream", llvm::cl::init(false),
    llvm::cl::desc("Nest inner streams with outer streams."));

llvm::cl::opt<bool> StreamPassEnableLoopBoundPredication(
    "stream-pass-enable-loop-bound-predication", llvm::cl::init(false),
    llvm::cl::desc("Stream-based loop bound predication."));