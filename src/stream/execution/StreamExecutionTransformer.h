#ifndef GEM_FORGE_STREAM_EXECUTION_TRANSFORMER_H
#define GEM_FORGE_STREAM_EXECUTION_TRANSFORMER_H

/**
 * This class transforms the module according to the provided
 * StreamRegionAnalyzer.
 */

#include "stream/StreamRegionAnalyzer.h"

#include "llvm/Analysis/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <unordered_set>
#include <vector>

class StreamExecutionTransformer {
public:
  StreamExecutionTransformer(
      llvm::Module *_Module, CachedLoopInfo *_CachedLI,
      std::string _OutputExtraFolderPath, bool _TransformTextMode,
      const std::vector<StreamRegionAnalyzer *> &Analyzers);

private:
  llvm::Module *Module;
  CachedLoopInfo *CachedLI;
  std::string OutputExtraFolderPath;
  bool TransformTextMode;

  /**
   * The trasnformed module.
   * Since the original module is heavily referenced by the datagraph, we clone
   * it and modify the new one.
   */
  std::unique_ptr<llvm::Module> ClonedModule;
  std::string ClonedModuleBCPath;
  std::string ClonedModuleLLPath;

  llvm::ValueToValueMapTy ClonedValueMap;
  std::unique_ptr<CachedLoopInfo> ClonedCachedLI;
  std::unique_ptr<llvm::DataLayout> ClonedDataLayout;

  // Instructions waiting to be removed at the end.
  std::unordered_set<llvm::Instruction *> PendingRemovedInsts;

  // All transformed functions.
  std::unordered_set<const llvm::Function *> TransformedFunctions;

  /**
   * All the configured stream regions, in the order of configured.
   * Used to generate allStreamRegion.
   * * There should be no duplicate in this vector.
   */
  std::vector<const StreamConfigureLoopInfo *> AllConfiguredLoopInfos;

  void transformStreamRegion(StreamRegionAnalyzer *Analyzer);
  void configureStreamsAtLoop(StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop);
  void insertStreamConfigAtLoop(StreamRegionAnalyzer *Analyzer,
                                llvm::Loop *Loop,
                                llvm::Constant *ConfigIdxValue);
  void insertStreamEndAtLoop(llvm::Loop *Loop, llvm::Constant *ConfigIdxValue);
  void coalesceStreamsAtLoop(StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop);
  void transformLoadInst(StreamRegionAnalyzer *Analyzer,
                         llvm::LoadInst *LoadInst);
  void transformStoreInst(StreamRegionAnalyzer *Analyzer,
                          llvm::StoreInst *StoreInst);
  void transformStepInst(StreamRegionAnalyzer *Analyzer,
                         llvm::Instruction *StepInst);
  llvm::Instruction *findStepPosition(Stream *StepStream,
                                      llvm::Instruction *StepInst);
  void cleanClonedModule();

  /**
   * Utility functions.
   */
  template <typename T>
  std::enable_if_t<std::is_base_of<llvm::Value, T>::value, T *>
  getClonedValue(const T *Value) {
    auto ClonedValueIter = this->ClonedValueMap.find(Value);
    if (ClonedValueIter == this->ClonedValueMap.end()) {
      llvm::errs() << "Failed to find cloned value "
                   << Utils::formatLLVMValue(Value) << '\n';
    }

    assert(ClonedValueIter != this->ClonedValueMap.end() &&
           "Failed to find cloned value.");
    auto ClonedValue = llvm::dyn_cast<T>(ClonedValueIter->second);
    assert(ClonedValue != nullptr && "Mismatched cloned value type.");
    return ClonedValue;
  }

  llvm::Loop *getOrCreateLoopInClonedModule(const llvm::Loop *Loop);
  llvm::BasicBlock *getOrCreateLoopPreheaderInClonedModule(llvm::Loop *Loop);

  /**
   * Key function to generate input value of the stream configuration.
   */
  using InputValueVec = std::vector<llvm::Value *>;
  using ProtoStreamConfiguration = LLVM::TDG::IVPattern;
  void generateIVStreamConfiguration(Stream *S, llvm::Instruction *InsertBefore,
                                     InputValueVec &ClonedInputValues);
  void generateMemStreamConfiguration(Stream *S,
                                      llvm::Instruction *InsertBefore,
                                      InputValueVec &ClonedInputValues);
  void generateAddRecStreamConfiguration(
      const llvm::Loop *ClonedConfigureLoop,
      const llvm::Loop *ClonedInnerMostLoop,
      const llvm::SCEVAddRecExpr *ClonedAddRecSCEV,
      llvm::Instruction *InsertBefore, llvm::ScalarEvolution *ClonedSE,
      llvm::SCEVExpander *ClonedSEExpander, InputValueVec &ClonedInputValues,
      ProtoStreamConfiguration *ProtoConfiguration);
  void addStreamInputValue(const llvm::SCEV *ClonedSCEV, bool Signed,
                           llvm::Instruction *InsertBefore,
                           llvm::SCEVExpander *ClonedSEExpander,
                           InputValueVec &ClonedInputValues,
                           ProtoStreamConfiguration *ProtoConfiguration);

  void writeModule();
  void writeAllConfiguredRegions();
  void writeAllTransformedFunctions();
};

#endif