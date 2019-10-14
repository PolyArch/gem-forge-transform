#ifndef GEM_FORGE_STREAM_EXECUTION_PASS_H
#define GEM_FORGE_STREAM_EXECUTION_PASS_H

#include "stream/StreamPass.h"

#include "llvm/Analysis/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <unordered_set>
#include <vector>

/**
 * This pass will actually clone the module and transform it using identified
 * stream. This is used for execution-driven simulation.
 */

class StreamExecutionPass : public StreamPass {
 public:
  static char ID;
  StreamExecutionPass() : StreamPass(ID) {}

 protected:
  void transformStream() override;

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

  /**
   * Since we are purely static trasnformation, we have to first select
   * StreamRegionAnalyzer as our candidate. Here I sort them by the number of
   * dynamic memory accesses and exclude overlapped regions.
   */
  std::vector<StreamRegionAnalyzer *> selectStreamRegionAnalyzers();

  void transformStreamRegion(StreamRegionAnalyzer *Analyzer);
  void configureStreamsAtLoop(StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop);
  void insertStreamConfigAtLoop(StreamRegionAnalyzer *Analyzer,
                                llvm::Loop *Loop,
                                llvm::Constant *ConfigIdxValue);
  void insertStreamEndAtLoop(llvm::Loop *Loop, llvm::Constant *ConfigIdxValue);
  void transformLoadInst(StreamRegionAnalyzer *Analyzer,
                         llvm::LoadInst *LoadInst);
  void transformStoreInst(StreamRegionAnalyzer *Analyzer,
                          llvm::StoreInst *StoreInst);
  void transformStepInst(StreamRegionAnalyzer *Analyzer,
                         llvm::Instruction *StepInst);
  void cleanClonedModule();

  /**
   * Utility functions.
   */
  template <typename T>
  std::enable_if_t<std::is_base_of<llvm::Value, T>::value, T *> getClonedValue(
      const T *Value) {
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
