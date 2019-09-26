#include "stream/StreamPass.h"

#include "TransformUtils.h"

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include "google/protobuf/util/json_util.h"

#include <queue>
#include <unordered_set>

#define DEBUG_TYPE "StreamExecutionPass"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

namespace {

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
  llvm::ValueToValueMapTy ClonedValueMap;
  std::unique_ptr<CachedLoopInfo> ClonedCachedLI;

  // Instructions waiting to be removed at the end.
  std::unordered_set<llvm::Instruction *> PendingRemovedInsts;

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
  std::enable_if_t<std::is_base_of<llvm::Value, T>::value, T *>
  getClonedValue(const T *Value) {
    auto ClonedValueIter = this->ClonedValueMap.find(Value);
    LLVM_DEBUG({
      if (ClonedValueIter == this->ClonedValueMap.end()) {
        llvm::errs() << "Failed to find cloned value "
                     << Utils::formatLLVMValue(Value) << '\n';
      }
    });

    assert(ClonedValueIter != this->ClonedValueMap.end() &&
           "Failed to find cloned value.");
    auto ClonedValue = llvm::dyn_cast<T>(ClonedValueIter->second);
    assert(ClonedValue != nullptr && "Mismatched cloned value type.");
    return ClonedValue;
  }

  llvm::Loop *getOrCreateLoopInClonedModule(llvm::Loop *Loop);
  llvm::BasicBlock *getOrCreateLoopPreheaderInClonedModule(llvm::Loop *Loop);

  void writeModule();
  void writeAllConfiguredRegions();
};

void StreamExecutionPass::transformStream() {

  // First we select the StreamRegionAnalyzer we want to use.
  LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer.\n");
  auto SelectedStreamRegionAnalyzers = this->selectStreamRegionAnalyzers();

  // Copy the module.
  LLVM_DEBUG(llvm::errs() << "Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(*(this->Module), this->ClonedValueMap);
  // Initialize the CachedLI for the cloned module.
  this->ClonedCachedLI =
      std::make_unique<CachedLoopInfo>(this->ClonedModule.get());

  // Transform region.
  for (auto &SelectedRegion : SelectedStreamRegionAnalyzers) {
    this->transformStreamRegion(SelectedRegion);
  }

  // Clean up the module.
  LLVM_DEBUG(llvm::errs() << "Clean the module.\n");
  this->cleanClonedModule();

  // Generate the module.
  LLVM_DEBUG(llvm::errs() << "Write the module.\n");
  this->writeModule();
  this->writeAllConfiguredRegions();

  // So far we still need to generate the history for testing purpose.
  // TODO: Remove this once we are done.
  StreamPass::transformStream();
}

std::vector<StreamRegionAnalyzer *>
StreamExecutionPass::selectStreamRegionAnalyzers() {
  std::vector<StreamRegionAnalyzer *> AllRegions;

  AllRegions.reserve(this->LoopStreamAnalyzerMap.size());
  for (auto &LoopStreamAnalyzerPair : this->LoopStreamAnalyzerMap) {
    AllRegions.push_back(LoopStreamAnalyzerPair.second.get());
  }

  // Sort them by the number of dynamic memory accesses.
  std::sort(
      AllRegions.begin(), AllRegions.end(),
      [](const StreamRegionAnalyzer *A, const StreamRegionAnalyzer *B) -> bool {
        return A->getNumDynamicMemAccesses() > B->getNumDynamicMemAccesses();
      });

  // Remove overlapped regions.
  std::vector<StreamRegionAnalyzer *> NonOverlapRegions;
  for (auto Region : AllRegions) {
    bool Overlapped = false;
    auto TopLoop = Region->getTopLoop();
    for (auto SelectedRegion : NonOverlapRegions) {
      auto SelectedTopLoop = SelectedRegion->getTopLoop();
      if (TopLoop->contains(SelectedTopLoop) ||
          SelectedTopLoop->contains(TopLoop)) {
        Overlapped = true;
        break;
      }
    }
    if (!Overlapped) {
      LLVM_DEBUG(llvm::errs() << "Select StreamRegionAnalyzer "
                              << LoopUtils::getLoopId(TopLoop) << ".\n");
      NonOverlapRegions.push_back(Region);
    }
  }
  return NonOverlapRegions;
}

void StreamExecutionPass::writeModule() {
  auto ModuleBCPath = this->OutputExtraFolderPath + "/stream.ex.bc";
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(ModuleBCPath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  assert(!ModuleFStream.has_error() &&
         "Failed to open the cloned module bc file.");
  llvm::WriteBitcodeToFile(*this->ClonedModule, ModuleFStream);
  ModuleFStream.close();

  if (GemForgeOutputDataGraphTextMode) {
    /**
     * Write to text mode for debug purpose.
     */
    auto ModuleLLPath = this->OutputExtraFolderPath + "/stream.ex.ll";
    std::error_code EC;
    llvm::raw_fd_ostream ModuleFStream(ModuleLLPath, EC,
                                       llvm::sys::fs::OpenFlags::F_None);
    assert(!ModuleFStream.has_error() &&
           "Failed to open the cloned module ll file.");
    this->ClonedModule->print(ModuleFStream, nullptr);
    ModuleFStream.close();
  }
}

void StreamExecutionPass::writeAllConfiguredRegions() {
  ::LLVM::TDG::AllStreamRegions ProtoAllRegions;
  ProtoAllRegions.set_binary(this->Module->getModuleIdentifier());
  for (auto ConfiguredLoopInfo : this->AllConfiguredLoopInfos) {
    ProtoAllRegions.add_relative_paths(ConfiguredLoopInfo->getRelativePath());
  }

  auto AllRegionPath = this->OutputExtraFolderPath + "/all.stream.data";
  Gem5ProtobufSerializer Serializer(AllRegionPath);
  Serializer.serialize(ProtoAllRegions);
  if (GemForgeOutputDataGraphTextMode) {
    auto AllRegionJsonPath = this->OutputExtraFolderPath + "/all.stream.json";
    std::ofstream InfoTextFStream(AllRegionJsonPath);
    assert(InfoTextFStream.is_open() &&
           "Failed to open the output info text file.");
    std::string InfoJsonString;
    ::google::protobuf::util::MessageToJsonString(ProtoAllRegions,
                                                  &InfoJsonString);
    InfoTextFStream << InfoJsonString;
    InfoTextFStream.close();
  }
}

void StreamExecutionPass::transformStreamRegion(
    StreamRegionAnalyzer *Analyzer) {
  auto TopLoop = Analyzer->getTopLoop();
  LLVM_DEBUG(llvm::errs() << "Transform stream region "
                          << LoopUtils::getLoopId(TopLoop) << '\n');
  // BFS to iterate all loops and configure them.
  std::queue<llvm::Loop *> LoopQueue;
  LoopQueue.push(TopLoop);
  while (!LoopQueue.empty()) {
    auto CurrentLoop = LoopQueue.front();
    LoopQueue.pop();
    for (auto &SubLoop : CurrentLoop->getSubLoops()) {
      LoopQueue.push(SubLoop);
    }
    this->configureStreamsAtLoop(Analyzer, CurrentLoop);
  }

  // Insert address computation function in the cloned module.
  Analyzer->insertAddrFuncInModule(this->ClonedModule);

  // Transform each individual instruction.
  for (auto *BB : TopLoop->blocks()) {
    for (auto InstIter = BB->begin(), InstEnd = BB->end(); InstIter != InstEnd;
         ++InstIter) {
      auto Inst = &*InstIter;
      const auto &TransformPlan = Analyzer->getTransformPlanByInst(Inst);

      /**
       * Originally we proposed pseudo-register to further eliminate the
       * overhead of load/store instruction. However, it is functionally correct
       * to use a special stream-load/stream-store instruction to explicitly
       * interact with the stream buffer. For simplicity, we will use the later
       * approach here and leave pseudo-register to the future.
       *
       * However, the transform plan is built for using pseudo-registers for
       * iv/load streams (store streams still use stream-store instruction),
       * which means we have to be carefully here.
       *
       * TODO: Really implement pseudo-register.
       */

      /**
       * Replace use of load with StreamLoad.
       */
      if (auto LoadInst = llvm::dyn_cast<llvm::LoadInst>(Inst)) {
        this->transformLoadInst(Analyzer, LoadInst);
      }

      /**
       * Replace use of store with StreamStore.
       */
      else if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(Inst)) {
        this->transformStoreInst(Analyzer, StoreInst);
      }

      /**
       * Handle StreamStep.
       * For pointer-chasing, step must happen after the step load.
       */
      auto IsStep = TransformPlan.Plan == StreamTransformPlan::PlanT::STEP ||
                    TransformPlan.Plan == StreamTransformPlan::PlanT::LOAD_STEP;
      if (IsStep) {
        this->transformStepInst(Analyzer, Inst);
      }
    }
  }
}

void StreamExecutionPass::configureStreamsAtLoop(StreamRegionAnalyzer *Analyzer,
                                                 llvm::Loop *Loop) {
  const auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  if (ConfigureInfo.TotalConfiguredStreams == 0) {
    // No stream configured at this loop.
    return;
  }

  /**
   * Allocate an entry in the allStreamRegions.
   * Subject to the imm12 limit.
   */
  assert(this->AllConfiguredLoopInfos.size() < 4096 &&
         "Too many configured streams.");
  auto ConfigIdx = this->AllConfiguredLoopInfos.size();
  this->AllConfiguredLoopInfos.push_back(&ConfigureInfo);

  LLVM_DEBUG(llvm::errs() << "Configure loop " << LoopUtils::getLoopId(Loop)
                          << '\n');
  auto ConfigIdxValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
      ConfigIdx, false);
  this->insertStreamConfigAtLoop(Analyzer, Loop, ConfigIdxValue);
  this->insertStreamEndAtLoop(Loop, ConfigIdxValue);
}

void StreamExecutionPass::insertStreamConfigAtLoop(
    StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop,
    llvm::Constant *ConfigIdxValue) {
  const auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  /**
   * 1. Make sure the loop has a predecessor.
   *    TODO: Maybe this can be relaxed in the future.
   * 2. Create a preheader.
   * 3. Insert the configure instruction.
   * 4. Insert all the live-in read instruction.
   */
  assert(Loop->getLoopPredecessor() != nullptr &&
         "Loop without predecessor. Please investigate.");
  auto ClonedPreheader = this->getOrCreateLoopPreheaderInClonedModule(Loop);
  assert(ClonedPreheader != nullptr &&
         "Failed to get/create a preheader in cloned module.");
  // Simply insert one stream configure instruction before the terminator.
  auto ClonedPreheaderTerminator = ClonedPreheader->getTerminator();
  llvm::IRBuilder<> Builder(ClonedPreheaderTerminator);
  std::array<llvm::Value *, 1> StreamConfigArgs{ConfigIdxValue};
  Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_config),
      StreamConfigArgs);

  for (auto S : ConfigureInfo.getSortedStreams()) {
    for (auto Input : S->getInputValues()) {
      // This is a live in?
      LLVM_DEBUG(llvm::errs()
                 << "Insert StreamInput for Stream " << S->formatName()
                 << " Input " << Utils::formatLLVMValue(Input) << '\n');

      auto StreamId = S->getRegionStreamId();
      assert(StreamId >= 0 && StreamId < 64 &&
             "Illegal RegionStreamId for StreamInput.");
      auto StreamIdValue = llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
          StreamId, false);
      auto ClonedInput = this->getClonedValue(Input);
      std::array<llvm::Value *, 2> StreamInputArgs{StreamIdValue, ClonedInput};
      std::array<llvm::Type *, 1> StreamInputType{ClonedInput->getType()};
      auto StreamInputInst = Builder.CreateCall(
          llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                          llvm::Intrinsic::ID::ssp_stream_input,
                                          StreamInputType),
          StreamInputArgs);
    }
  }

  // Insert the StreamReady instruction.
  Builder.CreateCall(llvm::Intrinsic::getDeclaration(
      this->ClonedModule.get(), llvm::Intrinsic::ID::ssp_stream_ready));
}

void StreamExecutionPass::insertStreamEndAtLoop(
    llvm::Loop *Loop, llvm::Constant *ConfigIdxValue) {

  /**
   * 1. Format dedicated exit block.
   * 2. Insert StreamEnd at every exit block.
   */
  auto ClonedLoop = this->getOrCreateLoopInClonedModule(Loop);
  auto ClonedFunc = ClonedLoop->getHeader()->getParent();
  auto ClonedLI = this->ClonedCachedLI->getLoopInfo(ClonedFunc);
  auto ClonedDT = this->ClonedCachedLI->getDominatorTree(ClonedFunc);
  llvm::formDedicatedExitBlocks(ClonedLoop, ClonedDT, ClonedLI, nullptr, false);

  std::unordered_set<llvm::BasicBlock *> VisitedExitBlockSet;
  for (auto *BB : ClonedLoop->blocks()) {
    for (auto *SuccBB : llvm::successors(BB)) {
      if (ClonedLoop->contains(SuccBB)) {
        continue;
      }
      if (!VisitedExitBlockSet.insert(SuccBB).second) {
        // Already visited.
        continue;
      }
      llvm::IRBuilder<> Builder(SuccBB->getFirstNonPHI());
      std::array<llvm::Value *, 1> StreamEndArgs{ConfigIdxValue};
      Builder.CreateCall(
          llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                          llvm::Intrinsic::ID::ssp_stream_end),
          StreamEndArgs);
    }
  }
}

llvm::Loop *
StreamExecutionPass::getOrCreateLoopInClonedModule(llvm::Loop *Loop) {
  auto Func = Loop->getHeader()->getParent();
  auto ClonedFunc = this->getClonedValue(Func);
  auto ClonedLI = this->ClonedCachedLI->getLoopInfo(ClonedFunc);
  auto ClonedHeader = this->getClonedValue(Loop->getHeader());
  auto ClonedLoop = ClonedLI->getLoopFor(ClonedHeader);
  // A little sanity check.
  assert(ClonedLoop->getLoopDepth() == Loop->getLoopDepth() &&
         "Mismatch loop depth for cloned loop.");
  return ClonedLoop;
}

llvm::BasicBlock *
StreamExecutionPass::getOrCreateLoopPreheaderInClonedModule(llvm::Loop *Loop) {
  auto ClonedLoop = this->getOrCreateLoopInClonedModule(Loop);
  auto ClonedPreheader = ClonedLoop->getLoopPreheader();
  if (ClonedPreheader != nullptr) {
    // The original loop has a preheader.
    return ClonedPreheader;
  }

  LLVM_DEBUG(llvm::errs() << "Failed to find preheader, creating one.\n");
  auto ClonedFunc = ClonedLoop->getHeader()->getParent();
  auto ClonedLI = this->ClonedCachedLI->getLoopInfo(ClonedFunc);
  auto ClonedDT = this->ClonedCachedLI->getDominatorTree(ClonedFunc);
  // We pass LI and DT so that they are updated.
  auto NewPreheader = llvm::InsertPreheaderForLoop(ClonedLoop, ClonedDT,
                                                   ClonedLI, nullptr, false);

  return NewPreheader;
}

void StreamExecutionPass::transformLoadInst(StreamRegionAnalyzer *Analyzer,
                                            llvm::LoadInst *LoadInst) {
  auto S = Analyzer->getChosenStreamByInst(LoadInst);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  /**
   * 1. Insert a StreamLoad.
   * 2. Replace the use of original load to StreamLoad.
   * 3. Leave the original load there (not deleted).
   */
  auto ClonedLoadInst = this->getClonedValue(LoadInst);
  llvm::IRBuilder<> Builder(ClonedLoadInst);

  // Here we should RegionStreamId to fit in immediate field.
  auto StreamId = S->getRegionStreamId();
  assert(StreamId >= 0 && StreamId < 64 &&
         "Illegal RegionStreamId for StreamLoad.");
  auto StreamIdValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()), StreamId,
      false);
  std::array<llvm::Value *, 1> StreamLoadArgs{StreamIdValue};

  auto LoadType = LoadInst->getType();
  auto StreamLoadType = LoadType;
  bool NeedTruncate = false;
  /**
   * ! It takes an effort to promote the loaded value to 64-bit in RV64,
   * ! so as a temporary fix, I truncate it here.
   */
  if (auto IntType = llvm::dyn_cast<llvm::IntegerType>(LoadType)) {
    if (IntType->getBitWidth() < 64) {
      StreamLoadType =
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
      NeedTruncate = true;
    } else if (IntType->getBitWidth() > 64) {
      assert(false && "Cannot handle load type larger than 64 bit.");
    }
  }

  std::array<llvm::Type *, 1> StreamLoadTypes{StreamLoadType};
  auto StreamLoadInst = Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_load,
                                      StreamLoadTypes),
      StreamLoadArgs);

  if (NeedTruncate) {
    auto TruncateInst = Builder.CreateTrunc(StreamLoadInst, LoadType);
    ClonedLoadInst->replaceAllUsesWith(TruncateInst);
  } else {
    ClonedLoadInst->replaceAllUsesWith(StreamLoadInst);
  }

  // Insert the load into the pending removed set.
  this->PendingRemovedInsts.insert(ClonedLoadInst);
}

void StreamExecutionPass::transformStoreInst(StreamRegionAnalyzer *Analyzer,
                                             llvm::StoreInst *StoreInst) {
  auto S = Analyzer->getChosenStreamByInst(StoreInst);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  assert(false && "StoreStream not supported yet.");
}

void StreamExecutionPass::transformStepInst(StreamRegionAnalyzer *Analyzer,
                                            llvm::Instruction *StepInst) {
  auto ClonedStepInst = this->getClonedValue(StepInst);
  llvm::IRBuilder<> Builder(ClonedStepInst);

  const auto &TransformPlan = Analyzer->getTransformPlanByInst(StepInst);
  for (auto StepStream : TransformPlan.getStepStreams()) {
    auto StreamId = StepStream->getRegionStreamId();
    assert(StreamId >= 0 && StreamId < 64 &&
           "Illegal RegionStreamId for StreamStep.");
    auto StreamIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
        StreamId, false);
    std::array<llvm::Value *, 1> StreamStepArgs{StreamIdValue};
    auto StreamStepInst = Builder.CreateCall(
        llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                        llvm::Intrinsic::ID::ssp_stream_step),
        StreamStepArgs);
  }
}
void StreamExecutionPass::cleanClonedModule() {
  // ! After this point, the ClonedValueMap is no longer injective.
  for (auto *Inst : this->PendingRemovedInsts) {
    /**
     * There are some instructions that may not be eliminated by the DCE,
     * e.g. the replaced store instruction.
     * So we delete them here.
     */
    assert(Inst->use_empty() && "PendingRemoveInst has use.");
    Inst->eraseFromParent();
  }
  this->PendingRemovedInsts.clear();

  /**
   * Chairman Mao once said, the dust won't go away by itself unless you sweep
   * it. Let's invoke the DCE to clean the module.
   */
  for (auto &ClonedFunc : this->ClonedModule->functions()) {
    if (ClonedFunc.isIntrinsic()) {
      continue;
    }
    if (ClonedFunc.isDeclaration()) {
      continue;
    }
    TransformUtils::eliminateDeadCode(
        ClonedFunc, this->ClonedCachedLI->getTargetLibraryInfo());
  }
}

} // namespace

#undef DEBUG_TYPE

char StreamExecutionPass::ID = 0;
static llvm::RegisterPass<StreamExecutionPass>
    X("stream-execution-pass", "Stream execution transform pass",
      false /* CFGOnly */, false /* IsAnalysis */);