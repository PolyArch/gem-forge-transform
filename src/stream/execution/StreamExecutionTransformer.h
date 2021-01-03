#ifndef GEM_FORGE_STREAM_EXECUTION_TRANSFORMER_H
#define GEM_FORGE_STREAM_EXECUTION_TRANSFORMER_H

/**
 * This class transforms the module according to the provided
 * StreamRegionAnalyzer.
 */

#include "stream/StaticStreamRegionAnalyzer.h"

#include "llvm/Analysis/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <unordered_set>
#include <vector>

class StreamExecutionTransformer {
public:
  StreamExecutionTransformer(
      llvm::Module *_Module,
      const std::unordered_set<llvm::Function *> &_ROIFunctions,
      CachedLoopInfo *_CachedLI, std::string _OutputExtraFolderPath,
      bool _TransformTextMode,
      const std::vector<StaticStreamRegionAnalyzer *> &Analyzers);

private:
  llvm::Module *Module;
  const std::unordered_set<llvm::Function *> &ROIFunctions;
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

  // Map from Stream to StreamLoad instructions.
  std::unordered_map<StaticStream *, llvm::Instruction *>
      StreamToStreamLoadInstMap;

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

  void transformStreamRegion(StaticStreamRegionAnalyzer *Analyzer);
  void configureStreamsAtLoop(StaticStreamRegionAnalyzer *Analyzer,
                              llvm::Loop *Loop);
  void insertStreamConfigAtLoop(StaticStreamRegionAnalyzer *Analyzer,
                                llvm::Loop *Loop,
                                llvm::Constant *ConfigIdxValue);
  void insertStreamEndAtLoop(StaticStreamRegionAnalyzer *Analyzer,
                             llvm::Loop *Loop, llvm::Constant *ConfigIdxValue);
  void insertStreamReduceAtLoop(StaticStreamRegionAnalyzer *Analyzer,
                                llvm::Loop *Loop, StaticStream *ReduceStream);
  void transformLoadInst(StaticStreamRegionAnalyzer *Analyzer,
                         llvm::Instruction *LoadInst);
  void transformStoreInst(StaticStreamRegionAnalyzer *Analyzer,
                          llvm::StoreInst *StoreInst);
  void transformAtomicRMWInst(StaticStreamRegionAnalyzer *Analyzer,
                              llvm::AtomicRMWInst *AtomicRMW);
  void transformAtomicCmpXchgInst(StaticStreamRegionAnalyzer *Analyzer,
                                  llvm::AtomicCmpXchgInst *AtomicCmpXchg);
  void transformStepInst(StaticStreamRegionAnalyzer *Analyzer,
                         llvm::Instruction *StepInst);
  void upgradeLoadToUpdateStream(StaticStreamRegionAnalyzer *Analyzer,
                                 StaticStream *LoadSS);
  void mergePredicatedStreams(StaticStreamRegionAnalyzer *Analyzer,
                              StaticStream *LoadSS);
  void mergePredicatedStore(StaticStreamRegionAnalyzer *Analyzer,
                            StaticStream *LoadSS, StaticStream *StoreSS,
                            bool PredTrue);
  void handleValueDG(StaticStreamRegionAnalyzer *Analyzer, StaticStream *S);
  llvm::Instruction *findStepPosition(StaticStream *StepStream,
                                      llvm::Instruction *StepInst);
  void cleanClonedModule();
  void removePendingRemovedInstsInFunc(
      llvm::Function &ClonedFunc,
      std::list<llvm::Instruction *> &PendingRemovedInsts);

  void replaceWithStreamMemIntrinsic();

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
  using ProtoStreamParam = ::LLVM::TDG::StreamParam;
  void generateIVStreamConfiguration(StaticStream *S,
                                     llvm::Instruction *InsertBefore,
                                     InputValueVec &ClonedInputValues);
  void generateMemStreamConfiguration(StaticStream *S,
                                      llvm::Instruction *InsertBefore,
                                      InputValueVec &ClonedInputValues);
  void generateUserMemStreamConfiguration(StaticStream *S,
                                          llvm::Instruction *InsertBefore,
                                          InputValueVec &ClonedInputValues);
  void generateAddRecStreamConfiguration(
      const llvm::Loop *ClonedConfigureLoop,
      const llvm::Loop *ClonedInnerMostLoop,
      const llvm::SCEVAddRecExpr *ClonedAddRecSCEV,
      llvm::Instruction *InsertBefore, llvm::ScalarEvolution *ClonedSE,
      llvm::SCEVExpander *ClonedSEExpander, InputValueVec &ClonedInputValues,
      ProtoStreamConfiguration *ProtoConfiguration);
  void handleExtraInputValue(StaticStream *SS,
                             InputValueVec &ClonedInputValues);
  void addStreamInputSCEV(const llvm::SCEV *ClonedSCEV, bool Signed,
                          llvm::Instruction *InsertBefore,
                          llvm::SCEVExpander *ClonedSEExpander,
                          InputValueVec &ClonedInputValues,
                          ProtoStreamConfiguration *ProtoConfiguration);
  void addStreamInputValue(const llvm::Value *ClonedValue, bool Signed,
                           InputValueVec &ClonedInputValues,
                           ProtoStreamParam *ProtoParam);
  llvm::Value *addStreamLoad(StaticStream *S, llvm::Type *LoadType,
                             llvm::Instruction *ClonedInsertBefore,
                             const llvm::DebugLoc *DebugLoc = nullptr);
  void addStreamStore(StaticStream *S, llvm::Instruction *ClonedInsertBefore,
                      const llvm::DebugLoc *DebugLoc = nullptr);

  void writeModule();
  void writeAllConfiguredRegions();
  void writeAllTransformedFunctions();
};

#endif