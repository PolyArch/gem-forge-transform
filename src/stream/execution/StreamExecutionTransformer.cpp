#include "StreamExecutionTransformer.h"

#include "TransformUtils.h"

#include "llvm/Analysis/CFG.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

#include <queue>

#define DEBUG_TYPE "StreamExecutionTransformer"

/**
 * Debug flag to control whether emit specific instruction.
 * ! Not used.
 */
#define GENERATE_STREAM_CONFIG
#define GENERATE_STREAM_INPUT
#define GENERATE_STREAM_READY
#define GENERATE_STREAM_STEP
#define GENERATE_STREAM_LOAD
#define GENERATE_STREAM_END

StreamExecutionTransformer::StreamExecutionTransformer(
    llvm::Module *_Module, CachedLoopInfo *_CachedLI,
    std::string _OutputExtraFolderPath, bool _TransformTextMode,
    const std::vector<StreamRegionAnalyzer *> &Analyzers)
    : Module(_Module), CachedLI(_CachedLI),
      OutputExtraFolderPath(_OutputExtraFolderPath),
      TransformTextMode(_TransformTextMode) {
  // Copy the module.
  LLVM_DEBUG(llvm::errs() << "Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(*(this->Module), this->ClonedValueMap);
  this->ClonedDataLayout =
      std::make_unique<llvm::DataLayout>(this->ClonedModule.get());
  // Initialize the CachedLI for the cloned module.
  this->ClonedCachedLI =
      std::make_unique<CachedLoopInfo>(this->ClonedModule.get());
  this->ClonedModuleBCPath = this->OutputExtraFolderPath + "/ex.bc";
  this->ClonedModuleLLPath = this->OutputExtraFolderPath + "/ex.ll";

  // Transform region.
  for (auto &Analyzer : Analyzers) {
    this->transformStreamRegion(Analyzer);
  }

  // Clean up the module.
  LLVM_DEBUG(llvm::errs() << "Clean the module.\n");
  this->cleanClonedModule();

  // Generate the module.
  LLVM_DEBUG(llvm::errs() << "Write the module.\n");
  this->writeModule();
  this->writeAllConfiguredRegions();
  // * Must be called after writeModule().
  this->writeAllTransformedFunctions();
}

void StreamExecutionTransformer::writeModule() {
  std::error_code EC;
  llvm::raw_fd_ostream ModuleFStream(this->ClonedModuleBCPath, EC,
                                     llvm::sys::fs::OpenFlags::F_None);
  assert(!ModuleFStream.has_error() &&
         "Failed to open the cloned module bc file.");
  llvm::WriteBitcodeToFile(*this->ClonedModule, ModuleFStream);
  ModuleFStream.close();

  if (this->TransformTextMode) {
    /**
     * Write to text mode for debug purpose.
     */
    std::error_code EC;
    llvm::raw_fd_ostream ModuleFStream(this->ClonedModuleLLPath, EC,
                                       llvm::sys::fs::OpenFlags::F_None);
    assert(!ModuleFStream.has_error() &&
           "Failed to open the cloned module ll file.");
    this->ClonedModule->print(ModuleFStream, nullptr);
    ModuleFStream.close();
  }
}

void StreamExecutionTransformer::writeAllConfiguredRegions() {
  ::LLVM::TDG::AllStreamRegions ProtoAllRegions;
  ProtoAllRegions.set_binary(this->Module->getModuleIdentifier());
  for (auto ConfiguredLoopInfo : this->AllConfiguredLoopInfos) {
    ProtoAllRegions.add_relative_paths(ConfiguredLoopInfo->getRelativePath());
  }

  auto AllRegionPath = this->OutputExtraFolderPath + "/all.stream.data";
  Gem5ProtobufSerializer Serializer(AllRegionPath);
  Serializer.serialize(ProtoAllRegions);
  if (this->TransformTextMode) {
    auto AllRegionJsonPath = this->OutputExtraFolderPath + "/all.stream.json";
    Utils::dumpProtobufMessageToJson(ProtoAllRegions, AllRegionJsonPath);
  }
}

void StreamExecutionTransformer::writeAllTransformedFunctions() {
  auto TransformedFunctionFolder = this->OutputExtraFolderPath + "/funcs";
  auto ErrCode = llvm::sys::fs::create_directory(TransformedFunctionFolder);
  if (ErrCode) {
    llvm::errs() << "Failed to create TransformedFunctionFolder: "
                 << TransformedFunctionFolder
                 << ". Reason: " << ErrCode.message() << '\n';
  }
  assert(!ErrCode && "Failed to create TransformedFunctionFolder.");
  for (auto Func : this->TransformedFunctions) {
    auto ClonedFunc = this->getClonedValue(Func);
    std::string TransformedLL = TransformedFunctionFolder + "/stream." +
                                ClonedFunc->getName().str() + ".ll";
    std::string ExtractCMD;
    llvm::raw_string_ostream ExtractCMDSS(ExtractCMD);
    ExtractCMDSS << "llvm-extract -func " << ClonedFunc->getName() << " -o "
                 << TransformedLL << " -S " << this->ClonedModuleBCPath;
    // We need to flush it to the string.
    ExtractCMDSS.str();
    assert(system(ExtractCMD.c_str()) == 0 &&
           "Failed to write transformed function.");
  }
}

void StreamExecutionTransformer::transformStreamRegion(
    StreamRegionAnalyzer *Analyzer) {
  auto TopLoop = Analyzer->getTopLoop();
  LLVM_DEBUG(llvm::errs() << "Transform stream region "
                          << LoopUtils::getLoopId(TopLoop) << '\n');

  // Insert the function into the transformed set.
  this->TransformedFunctions.insert(TopLoop->getHeader()->getParent());

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
    this->coalesceStreamsAtLoop(Analyzer, CurrentLoop);
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

void StreamExecutionTransformer::configureStreamsAtLoop(
    StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop) {
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

void StreamExecutionTransformer::coalesceStreamsAtLoop(
    StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop) {
  const auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  if (ConfigureInfo.TotalConfiguredStreams == 0) {
    // No streams here.
    return;
  }
  /**
   * We coalesce memory streams if:
   * 1. They share the same step root.
   * 2. They have scev.
   * 3. Their SCEV has a constant offset within a cache line (64 bytes).
   */
  std::list<std::vector<std::pair<Stream *, int64_t>>> CoalescedGroup;
  for (auto S : ConfigureInfo.getSortedStreams()) {
    if (S->SStream->Type == StaticStream::TypeT::IV) {
      continue;
    }
    if (S->getBaseStepRootStreams().size() != 1) {
      continue;
    }
    LLVM_DEBUG(llvm::errs()
               << "====== Try to coalesce stream: " << S->formatName() << '\n');
    auto SS = S->SStream;
    auto Addr = const_cast<llvm::Value *>(Utils::getMemAddrValue(SS->Inst));
    auto AddrSCEV = SS->SE->getSCEV(Addr);

    bool Coalesced = false;
    if (AddrSCEV) {
      LLVM_DEBUG(llvm::errs() << "AddrSCEV: "; AddrSCEV->dump());
      for (auto &Group : CoalescedGroup) {
        auto TargetS = Group.front().first;
        if (TargetS->getBaseStepRootStreams() != S->getBaseStepRootStreams()) {
          // Do not share the same step root.
          continue;
        }
        auto TargetSS = TargetS->SStream;
        // Check the load/store.
        if (TargetSS->Inst->getOpcode() != SS->Inst->getOpcode()) {
          continue;
        }
        auto TargetAddr =
            const_cast<llvm::Value *>(Utils::getMemAddrValue(TargetSS->Inst));
        auto TargetAddrSCEV = TargetSS->SE->getSCEV(TargetAddr);
        if (!TargetAddrSCEV) {
          continue;
        }
        // Check the scev.
        LLVM_DEBUG(llvm::errs() << "TargetAddrSCEV: "; TargetAddrSCEV->dump());
        auto MinusSCEV = SS->SE->getMinusSCEV(AddrSCEV, TargetAddrSCEV);
        LLVM_DEBUG(llvm::errs() << "MinusSCEV: "; MinusSCEV->dump());
        auto OffsetSCEV = llvm::dyn_cast<llvm::SCEVConstant>(MinusSCEV);
        if (!OffsetSCEV) {
          // Not constant offset.
          continue;
        }
        LLVM_DEBUG(llvm::errs() << "OffsetSCEV: "; OffsetSCEV->dump());
        int64_t Offset = OffsetSCEV->getAPInt().getSExtValue();
        if (Offset > 64 || Offset < -64) {
          continue;
        }
        LLVM_DEBUG(llvm::errs()
                   << "Coalesced, offset: " << Offset
                   << " with stream: " << TargetS->formatName() << '\n');
        Coalesced = true;
        Group.emplace_back(S, Offset);
        break;
      }
    }

    if (!Coalesced) {
      LLVM_DEBUG(llvm::errs() << "New coalesce group\n");
      CoalescedGroup.emplace_back();
      CoalescedGroup.back().emplace_back(S, 0);
    }
  }

  // Find the lowest one as the base for each group.
  for (auto &Group : CoalescedGroup) {
    if (Group.size() == 1) {
      // Ignore single streams.
      continue;
    }
    auto BaseS = Group.front().first;
    auto MinOffset = Group.front().second;
    for (const auto &Entry : Group) {
      if (Entry.second < MinOffset) {
        MinOffset = Entry.second;
        BaseS = Entry.first;
      }
    }
    // Generate the coalesced info.
    for (auto &Entry : Group) {
      auto S = Entry.first;
      S->setCoalesceGroup(BaseS->getStreamId(), Entry.second - MinOffset);
    }
  }
}

void StreamExecutionTransformer::insertStreamConfigAtLoop(
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
    InputValueVec ClonedInputValues;
    if (S->SStream->Type == StaticStream::TypeT::IV) {
      this->generateIVStreamConfiguration(S, ClonedPreheaderTerminator,
                                          ClonedInputValues);
    } else {
      this->generateMemStreamConfiguration(S, ClonedPreheaderTerminator,
                                           ClonedInputValues);
    }
    for (auto ClonedInput : ClonedInputValues) {
      // This is a live in?
      LLVM_DEBUG(llvm::errs()
                 << "Insert StreamInput for Stream " << S->formatName()
                 << " Input " << ClonedInput->getName() << '\n');

      auto StreamId = S->getRegionStreamId();
      assert(StreamId >= 0 && StreamId < 128 &&
             "Illegal RegionStreamId for StreamInput.");
      auto StreamIdValue = llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
          StreamId, false);
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

void StreamExecutionTransformer::insertStreamEndAtLoop(
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

llvm::Loop *StreamExecutionTransformer::getOrCreateLoopInClonedModule(
    const llvm::Loop *Loop) {
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
StreamExecutionTransformer::getOrCreateLoopPreheaderInClonedModule(
    llvm::Loop *Loop) {
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

void StreamExecutionTransformer::transformLoadInst(
    StreamRegionAnalyzer *Analyzer, llvm::LoadInst *LoadInst) {
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
  assert(StreamId >= 0 && StreamId < 128 &&
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

void StreamExecutionTransformer::transformStoreInst(
    StreamRegionAnalyzer *Analyzer, llvm::StoreInst *StoreInst) {
  auto S = Analyzer->getChosenStreamByInst(StoreInst);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  /**
   * For execution-driven simulation, to aovid complicated interaction
   * with the core's LSQ, we make store stream implicit so far,
   * i.e. the original store is kept there, and the data is consumed
   * through the cache.
   */
}

void StreamExecutionTransformer::transformStepInst(
    StreamRegionAnalyzer *Analyzer, llvm::Instruction *StepInst) {
  const auto &TransformPlan = Analyzer->getTransformPlanByInst(StepInst);
  for (auto StepStream : TransformPlan.getStepStreams()) {

    auto StepPosition = this->findStepPosition(StepStream, StepInst);
    auto ClonedStepPosition = this->getClonedValue(StepPosition);
    llvm::IRBuilder<> Builder(ClonedStepPosition);

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

llvm::Instruction *
StreamExecutionTransformer::findStepPosition(Stream *StepStream,
                                             llvm::Instruction *StepInst) {
  /**
   * It is tricky to find the correct insertion point of the step instruction.
   * It changes the meaning of the induction variable as well as all dependent
   * streams.
   * One naive solution is to insert StreamStep after the increment instruction,
   * usually like i++. However, when the loop body contains a[i+1], the compiler
   * will try to avoid compute i+1 twice. Thus, it will generate IR like these:
   *
   * next.i = i + 1
   * value  = load a[next.i]
   * ended  = cmp next.i, N
   * br
   *
   * Therefore, inserting StreamStep after next.i will cause the next StreamLoad
   * to be stepped early. I have no general solution so far, but luckily, in the
   * current implementation, the original induction variable is not removed,
   * which simplifies the semantic of StreamStep. It only controls the memory
   * streams, and the induction variable streams are not exposed to the original
   * program. So I came up with a adhoc solution.
   * 1. Check if the basic block dominates all loop latches. If so, then this is
   * an uncondtionally step, and we simply insert one StreamStep before the
   * terminator of all loop latches.
   * 2. If not, then this is a conditional step. I am not sure how to handle
   * this yet, but we perform a sanity check if any dependent stream load comes
   * after the basic block. If no StreamLoad comes after me, then we simply
   * insert the StreamStep before the terminator of the step basic block.
   */
  auto Loop = StepStream->SStream->InnerMostLoop;
  auto LI = this->CachedLI->getLoopInfo(StepInst->getFunction());
  auto DT = this->CachedLI->getDominatorTree(StepInst->getFunction());
  auto StepBB = StepInst->getParent();
  auto Latch = Loop->getLoopLatch();
  /**
   * So far only consider the case with single latch.
   * Multiple latches is compilcated, if one latch can lead to another latch.
   */
  if (!Latch) {
    LLVM_DEBUG(llvm::errs() << "Multiple latches for "
                            << LoopUtils::getLoopId(Loop) << '\n');
  }
  if (DT->dominates(StepBB, Latch)) {
    // Simple case.
    return Latch->getTerminator();
  }

  // Sanity check for all dependent streams.
  std::unordered_set<llvm::BasicBlock *> DependentStreamBBs;
  std::queue<Stream *> DependentStreamQueue;
  for (auto S : StepStream->getDependentStreams()) {
    DependentStreamQueue.push(S);
  }
  while (!DependentStreamQueue.empty()) {
    auto S = DependentStreamQueue.front();
    DependentStreamQueue.pop();
    auto BB = const_cast<llvm::BasicBlock *>(S->SStream->Inst->getParent());
    if (BB == StepBB) {
      // StepBB is fine.
      continue;
    }
    DependentStreamBBs.insert(BB);
    for (auto DepS : S->getDependentStreams()) {
      DependentStreamQueue.push(DepS);
    }
  }
  // Check if it is reachable from StepBB to DependentBB.
  llvm::SmallVector<llvm::BasicBlock *, 1> StartBB = {StepBB};
  llvm::SmallPtrSet<llvm::BasicBlock *, 1> ExclusionBB = {Latch};
  for (auto DepBB : DependentStreamBBs) {
    if (llvm::isPotentiallyReachableFromMany(StartBB, DepBB, &ExclusionBB, DT,
                                             LI)) {
      llvm::errs() << "Potential MemStream after StreamStep "
                   << StepStream->formatName() << " BB " << DepBB->getName()
                   << '\n';
      assert(false && "MemStream after Step.");
    }
  }
  // Passed sanity check.
  return StepBB->getTerminator();
}

void StreamExecutionTransformer::cleanClonedModule() {
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

void StreamExecutionTransformer::generateIVStreamConfiguration(
    Stream *S, llvm::Instruction *InsertBefore,
    std::vector<llvm::Value *> &ClonedInputValues) {
  assert(S->SStream->Type == StaticStream::TypeT::IV && "Must be IV stream.");
  auto SS = static_cast<const StaticIndVarStream *>(S->SStream);
  auto ProtoConfiguration = SS->StaticStreamInfo.mutable_iv_pattern();

  auto PHINode = SS->PHINode;
  auto ClonedPHINode = this->getClonedValue(PHINode);
  auto ClonedSE =
      this->ClonedCachedLI->getScalarEvolution(ClonedPHINode->getFunction());
  auto ClonedSEExpander =
      this->ClonedCachedLI->getSCEVExpander(ClonedPHINode->getFunction());
  auto ClonedConfigureLoop =
      this->getOrCreateLoopInClonedModule(SS->ConfigureLoop);
  auto ClonedInnerMostLoop =
      this->getOrCreateLoopInClonedModule(SS->InnerMostLoop);

  auto PHINodeSCEV = SS->SE->getSCEV(SS->PHINode);
  auto ClonedPHINodeSCEV = ClonedSE->getSCEV(ClonedPHINode);
  LLVM_DEBUG({
    llvm::errs() << "====== Generate IVStreamConfiguration "
                 << S->SStream->formatName() << " PHINode ";
    PHINodeSCEV->dump();
  });

  if (auto PHINodeAddRecSCEV =
          llvm::dyn_cast<llvm::SCEVAddRecExpr>(PHINodeSCEV)) {
    auto ClonedPHINodeAddRecSCEV =
        llvm::dyn_cast<llvm::SCEVAddRecExpr>(ClonedPHINodeSCEV);
    assert(ClonedPHINodeAddRecSCEV && "Cloned SCEV is not AddRec anymore.");
    this->generateAddRecStreamConfiguration(
        ClonedConfigureLoop, ClonedInnerMostLoop, ClonedPHINodeAddRecSCEV,
        InsertBefore, ClonedSE, ClonedSEExpander, ClonedInputValues,
        ProtoConfiguration);
  } else {
    assert(false && "Can't handle this PHI node.");
  }
}

void StreamExecutionTransformer::generateMemStreamConfiguration(
    Stream *S, llvm::Instruction *InsertBefore,
    InputValueVec &ClonedInputValues) {

  LLVM_DEBUG(llvm::errs() << "===== Generate MemStreamConfiguration "
                          << S->formatName() << '\n');

  /**
   * Some sanity check here.
   */
  assert(S->getChosenBaseStepRootStreams().size() == 1 &&
         "Missing step root stream.");
  for (auto StepRootS : S->getChosenBaseStepRootStreams()) {
    if (StepRootS->SStream->ConfigureLoop != S->SStream->ConfigureLoop) {
      llvm::errs() << "StepRootStream is not configured at the same loop: "
                   << S->formatName() << '\n';
      assert(false);
    }
  }

  assert(S->SStream->Type == StaticStream::TypeT::MEM && "Must be MEM stream.");
  auto SS = static_cast<const StaticMemStream *>(S->SStream);
  auto ProtoConfiguration = SS->StaticStreamInfo.mutable_iv_pattern();

  auto Addr = const_cast<llvm::Value *>(Utils::getMemAddrValue(SS->Inst));
  auto ClonedAddr = this->getClonedValue(Addr);
  auto ClonedFunction = this->getClonedValue(SS->Inst->getFunction());

  auto ClonedSE = this->ClonedCachedLI->getScalarEvolution(ClonedFunction);
  auto ClonedSEExpander = this->ClonedCachedLI->getSCEVExpander(ClonedFunction);

  auto AddrSCEV = SS->SE->getSCEV(Addr);

  LLVM_DEBUG({
    llvm::errs() << "Generate MemStreamConfiguration " << SS->formatName()
                 << " Addr ";
    AddrSCEV->dump();
  });

  if (llvm::isa<llvm::SCEVAddRecExpr>(AddrSCEV)) {
    auto ClonedConfigureLoop =
        this->getOrCreateLoopInClonedModule(SS->ConfigureLoop);
    auto ClonedInnerMostLoop =
        this->getOrCreateLoopInClonedModule(SS->InnerMostLoop);
    auto ClonedAddrSCEV = ClonedSE->getSCEV(ClonedAddr);
    auto ClonedAddrAddRecSCEV =
        llvm::dyn_cast<llvm::SCEVAddRecExpr>(ClonedAddrSCEV);
    this->generateAddRecStreamConfiguration(
        ClonedConfigureLoop, ClonedInnerMostLoop, ClonedAddrAddRecSCEV,
        InsertBefore, ClonedSE, ClonedSEExpander, ClonedInputValues,
        ProtoConfiguration);
    return;
  }
  if (SS->StaticStreamInfo.val_pattern() ==
      ::LLVM::TDG::StreamValuePattern::INDIRECT) {
    // Check if this is indirect stream.
    LLVM_DEBUG(llvm::errs() << "This is Indirect MemStream.\n");
    for (auto BaseS : S->getChosenBaseStreams()) {
      auto BaseSS = BaseS->SStream;
      if (BaseSS->ConfigureLoop != SS->ConfigureLoop ||
          BaseSS->InnerMostLoop != SS->InnerMostLoop) {
        llvm::errs() << "Cannot handle indirect streams with mismatch loop: \n";
        llvm::errs() << "Base: " << BaseSS->formatName() << '\n';
        llvm::errs() << "Dep:  " << SS->formatName() << '\n';
        assert(false && "Mismatch in configure loop for indirect streams.");
      }
    }
    // Set the IV pattern to indirect, and the stream will fill in AddrFunc
    // info.
    ProtoConfiguration->set_val_pattern(
        ::LLVM::TDG::StreamValuePattern::INDIRECT);
    // Handle the input values.
    for (auto Input : S->getInputValues()) {
      ClonedInputValues.push_back(this->getClonedValue(Input));
    }
    return;
  }
  assert(false && "Can't handle this Addr.");
}

void StreamExecutionTransformer::generateAddRecStreamConfiguration(
    const llvm::Loop *ClonedConfigureLoop,
    const llvm::Loop *ClonedInnerMostLoop,
    const llvm::SCEVAddRecExpr *ClonedAddRecSCEV,
    llvm::Instruction *InsertBefore, llvm::ScalarEvolution *ClonedSE,
    llvm::SCEVExpander *ClonedSEExpander, InputValueVec &ClonedInputValues,
    ProtoStreamConfiguration *ProtoConfiguration) {
  assert(ClonedAddRecSCEV->isAffine() &&
         "Can only handle affine AddRecSCEV so far.");
  ProtoConfiguration->set_val_pattern(::LLVM::TDG::StreamValuePattern::LINEAR);

  // ! These are all cloned SCEV.
  int RecurLevel = 0;
  const llvm::Loop *CurrentLoop = ClonedInnerMostLoop;
  const llvm::SCEV *CurrentSCEV = ClonedAddRecSCEV;
  // Peeling up nested loops.
  while (ClonedConfigureLoop->contains(CurrentLoop)) {
    RecurLevel++;
    LLVM_DEBUG({
      llvm::errs() << "Peeling " << CurrentLoop->getName() << ' ';
      CurrentSCEV->dump();
    });
    if (!ClonedSE->isLoopInvariant(CurrentSCEV, CurrentLoop)) {
      // We only allow AddRec for LoopVariant SCEV.
      if (auto AddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(CurrentSCEV)) {
        assert(AddRecSCEV->isAffine() && "Cannot handle Quadratic AddRecSCEV.");
        auto StrideSCEV = AddRecSCEV->getOperand(1);
        assert(ClonedSE->isLoopInvariant(StrideSCEV, ClonedConfigureLoop) &&
               "LoopVariant stride.");
        // Add the stride input.
        this->addStreamInputValue(StrideSCEV, true /* Signed */, InsertBefore,
                                  ClonedSEExpander, ClonedInputValues,
                                  ProtoConfiguration);
        auto StartSCEV = AddRecSCEV->getStart();
        CurrentSCEV = StartSCEV;
      } else {
        assert(false && "Cannot handle this LoopVariant SCEV.");
      }
    } else {
      /**
       * If this is LoopInvariant, we can add a 0 stride and do not update
       * the SCEV.
       */
      auto ZeroStrideSCEV = ClonedSE->getZero(
          llvm::IntegerType::get(this->ClonedModule->getContext(), 64));
      this->addStreamInputValue(ZeroStrideSCEV, true /* Signed */, InsertBefore,
                                ClonedSEExpander, ClonedInputValues,
                                ProtoConfiguration);
    }
    // We need the back-edge taken times if this is not ConfigureLoop.
    // Otherwise it's optional.
    auto BackEdgeTakenSCEV = ClonedSE->getBackedgeTakenCount(CurrentLoop);
    LLVM_DEBUG({
      llvm::errs() << "BackEdgeTakenCount ";
      BackEdgeTakenSCEV->dump();
    });
    if (!llvm::isa<llvm::SCEVCouldNotCompute>(BackEdgeTakenSCEV) &&
        ClonedSE->isLoopInvariant(BackEdgeTakenSCEV, ClonedConfigureLoop)) {
      this->addStreamInputValue(BackEdgeTakenSCEV, false /* Signed */,
                                InsertBefore, ClonedSEExpander,
                                ClonedInputValues, ProtoConfiguration);
    } else {
      assert(CurrentLoop == ClonedConfigureLoop &&
             "Need const BackEdgeTakenCount for nested loop.");
    }
    CurrentLoop = CurrentLoop->getParentLoop();
  }
  // Finally we should be left with the start value.
  LLVM_DEBUG({
    llvm::errs() << "StartSCEV ";
    CurrentSCEV->dump();
    llvm::errs() << '\n';
  });
  assert(ClonedSE->isLoopInvariant(CurrentSCEV, ClonedConfigureLoop) &&
         "LoopVariant StartValue.");
  this->addStreamInputValue(CurrentSCEV, false /*Signed */, InsertBefore,
                            ClonedSEExpander, ClonedInputValues,
                            ProtoConfiguration);
}

void StreamExecutionTransformer::addStreamInputValue(
    const llvm::SCEV *ClonedSCEV, bool Signed, llvm::Instruction *InsertBefore,
    llvm::SCEVExpander *ClonedSCEVExpander, InputValueVec &ClonedInputValues,
    LLVM::TDG::IVPattern *ProtoConfiguration) {
  auto ProtoInput = ProtoConfiguration->add_params();
  auto ClonedInputValue =
      ClonedSCEVExpander->expandCodeFor(ClonedSCEV, nullptr, InsertBefore);

  if (auto ConstInputValue =
          llvm::dyn_cast<llvm::ConstantInt>(ClonedInputValue)) {
    // This is constant.
    ProtoInput->set_valid(true);
    if (Signed) {
      ProtoInput->set_param(ConstInputValue->getSExtValue());
    } else {
      ProtoInput->set_param(ConstInputValue->getZExtValue());
    }
  } else {
    // Runtime input.
    ProtoInput->set_valid(false);
    ClonedInputValues.push_back(ClonedInputValue);
  }

  return;
}

#undef DEBUG_TYPE