#include "StreamExecutionTransformer.h"

#include "TransformUtils.h"

#include "llvm/Analysis/CFG.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Verifier.h"
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

  /**
   * BFS to iterate all loops and configure them.
   * ! This must be done at the end as UpgradeUpdate may add more inputs.
   */
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
  this->insertStreamEndAtLoop(Analyzer, Loop, ConfigIdxValue);
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

  // Sort each group with increasing order of offset.
  for (auto &Group : CoalescedGroup) {
    std::sort(Group.begin(), Group.end(),
              [](const std::pair<Stream *, int64_t> &a,
                 const std::pair<Stream *, int64_t> &b) -> bool {
                return a.second < b.second;
              });
    // Set the base/offset.
    auto BaseS = Group.front().first;
    int64_t BaseOffset = Group.front().second;
    for (auto &StreamOffset : Group) {
      auto S = StreamOffset.first;
      auto Offset = StreamOffset.second;
      S->setCoalesceGroup(BaseS->getStreamId(), Offset - BaseOffset);
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
      // Extend the input value to 64 bit using zero extension.
      auto ClonedInputValue = ClonedInput;
      auto ClonedInputType = ClonedInput->getType();
      if (auto IntType = llvm::dyn_cast<llvm::IntegerType>(ClonedInputType)) {
        if (IntType->getBitWidth() < 64) {
          ClonedInputType =
              llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
          ClonedInputValue = Builder.CreateZExt(ClonedInput, ClonedInputType);
        } else if (IntType->getBitWidth() > 64) {
          assert(false && "Cannot handle input type larger than 64 bit.");
        }
      }

      auto StreamId = S->getRegionStreamId();
      assert(StreamId >= 0 && StreamId < 128 &&
             "Illegal RegionStreamId for StreamInput.");
      auto StreamIdValue = llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
          StreamId, false);
      std::array<llvm::Value *, 2> StreamInputArgs{StreamIdValue,
                                                   ClonedInputValue};
      std::array<llvm::Type *, 1> StreamInputType{ClonedInputType};
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
    StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop,
    llvm::Constant *ConfigIdxValue) {
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

  /**
   * Handle reduction stream.
   * * Only do this after we create dedicated exit blocks and insert StreamEnd.
   */
  const auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  for (auto S : ConfigureInfo.getSortedStreams()) {
    this->insertStreamReduceAtLoop(Analyzer, Loop, S);
  }
}

void StreamExecutionTransformer::insertStreamReduceAtLoop(
    StreamRegionAnalyzer *Analyzer, llvm::Loop *Loop, Stream *ReduceStream) {

  auto SS = ReduceStream->SStream;
  const auto &SSInfo = SS->StaticStreamInfo;
  if (SSInfo.val_pattern() != ::LLVM::TDG::StreamValuePattern::REDUCTION) {
    return;
  }

  auto PHINode = llvm::dyn_cast<llvm::PHINode>(SS->Inst);
  assert(PHINode && "Reduction stream should be PHINode.");
  auto ExitValue = SS->ReduceDG->getResultValue();
  auto ExitInst = llvm::dyn_cast<llvm::Instruction>(ExitValue);
  assert(ExitInst &&
         "ExitValue should be an instruction for reduction stream.");
  // Add a final StreamLoad.
  auto ClonedLoop = this->getOrCreateLoopInClonedModule(Loop);
  // std::unordered_map<llvm::BasicBlock *, llvm::Value *>
  //     VisitedExitBlockValueMap;
  // for (auto *BB : ClonedLoop->blocks()) {
  //   for (auto *SuccBB : llvm::successors(BB)) {
  //     if (ClonedLoop->contains(SuccBB)) {
  //       continue;
  //     }
  //     if (!VisitedExitBlockValueMap.emplace(SuccBB, nullptr).second) {
  //       // Already visited.
  //       continue;
  //     }
  //     auto StreamLoad =
  //         this->addStreamLoad(ReduceStream, SuccBB->getFirstNonPHI());
  //     VisitedExitBlockValueMap.at(SuccBB) = StreamLoad;
  //   }
  // }

  /**
   * Let's only handle one exit block so far.
   * Try to replace out-of-loop user.
   */
  auto ClonedExitBB = ClonedLoop->getExitBlock();
  if (!ClonedExitBB) {
    return;
  }
  auto ClonedReducedValue =
      this->addStreamLoad(ReduceStream, ClonedExitBB->getFirstNonPHI());
  auto ClonedExitInst = this->getClonedValue(ExitInst);
  /**
   * Find out-of-loop ExitValue user instructions.
   * We have to first find them and then replace the usages, cause we can
   * not iterate through users() and modify it at the same time.
   */
  std::unordered_set<llvm::Instruction *> OutOfLoopClonedExitInstUsers;
  for (auto ClonedUser : ClonedExitInst->users()) {
    if (auto ClonedUserInst = llvm::dyn_cast<llvm::Instruction>(ClonedUser)) {
      LLVM_DEBUG(llvm::dbgs()
                 << "ReduceStream " << ReduceStream->formatName()
                 << " ExitInst " << ClonedExitInst->getName() << " user "
                 << ClonedUserInst->getName() << " Loop "
                 << ClonedLoop->getHeader()->getName() << " contains "
                 << ClonedLoop->contains(ClonedUserInst) << '\n');
      if (!ClonedLoop->contains(ClonedUserInst)) {
        // Ignore in loop user.
        OutOfLoopClonedExitInstUsers.insert(ClonedUserInst);
      }
    }
  }
  for (auto ClonedUserInst : OutOfLoopClonedExitInstUsers) {
    // This is an outside user, replace it.
    ClonedUserInst->replaceUsesOfWith(ClonedExitInst, ClonedReducedValue);
  }
  /**
   * After we replace the out-of-loop user, we check if the reduction
   * is complete, i.e. define a set of instruction as:
   * S = ComputeInsts U { ReductionPHINode }.
   * Check that:
   *   for I : S
   *     for user : I
   *       user in S
   * Since there is no other core user, we can remove the computation
   * from the core and purely let the StreamEngine handle it.
   */
  std::unordered_set<llvm::Instruction *> ClonedS = {
      this->getClonedValue(PHINode)};
  for (auto ComputeInst : SS->ReduceDG->getComputeInsts()) {
    ClonedS.insert(this->getClonedValue(ComputeInst));
  }
  bool IsComplete = true;
  for (auto ClonedInst : ClonedS) {
    for (auto User : ClonedInst->users()) {
      if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
        if (!ClonedS.count(UserInst)) {
          LLVM_DEBUG(llvm::dbgs()
                     << "ReduceStream " << ReduceStream->formatName()
                     << " is incomplete due to user " << UserInst->getName()
                     << " of ComputeInst " << ClonedInst->getName() << '\n');
          IsComplete = false;
          break;
        }
      }
    }
    if (!IsComplete) {
      break;
    }
  }
  if (IsComplete) {
    // If this is complete, we replace uses by UndefValue and
    // mark all instructions as PendingRemove.
    for (auto ClonedInst : ClonedS) {
      ClonedInst->replaceAllUsesWith(
          llvm::UndefValue::get(ClonedInst->getType()));
    }
    this->PendingRemovedInsts.insert(ClonedS.begin(), ClonedS.end());
    // Mark the reduction stream has no core user.
    ReduceStream->SStream->StaticStreamInfo.set_no_core_user(true);
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
  auto StreamLoadInst = this->addStreamLoad(S, ClonedLoadInst);
  ClonedLoadInst->replaceAllUsesWith(StreamLoadInst);

  // Insert the load into the pending removed set.
  this->PendingRemovedInsts.insert(ClonedLoadInst);

  this->upgradeLoadToUpdateStream(Analyzer, S);
  this->mergePredicatedStreams(Analyzer, S);
}

llvm::Value *StreamExecutionTransformer::addStreamLoad(
    Stream *S, llvm::Instruction *ClonedInsertBefore) {
  /**
   * Insert a StreamLoad for S.
   */
  auto Inst = S->SStream->Inst;

  // Here we should RegionStreamId to fit in immediate field.
  auto StreamId = S->getRegionStreamId();
  assert(StreamId >= 0 && StreamId < 128 &&
         "Illegal RegionStreamId for StreamLoad.");
  auto StreamIdValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()), StreamId,
      false);
  std::array<llvm::Value *, 1> StreamLoadArgs{StreamIdValue};

  auto LoadType = Inst->getType();
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
  llvm::IRBuilder<> Builder(ClonedInsertBefore);
  auto StreamLoadInst = Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_load,
                                      StreamLoadTypes),
      StreamLoadArgs);
  this->StreamToStreamLoadInstMap.emplace(S, StreamLoadInst);
  if (NeedTruncate) {
    auto TruncateInst = Builder.CreateTrunc(StreamLoadInst, LoadType);
    return TruncateInst;
  } else {
    return StreamLoadInst;
  }
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

  // Try to merge Load-Store DG.
  this->mergeLoadStoreDG(Analyzer, S);
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

void StreamExecutionTransformer::upgradeLoadToUpdateStream(
    StreamRegionAnalyzer *Analyzer, Stream *LoadStream) {
  /**
   * The idea is to update a load to update stream and thus must be offloaded.
   */
  if (!StreamPassUpgradeLoadToUpdate) {
    return;
  }
  auto LoadSS = LoadStream->SStream;
  auto &LoadSSInfo = LoadSS->StaticStreamInfo;
  if (!(LoadSSInfo.has_update() && LoadSSInfo.has_const_update())) {
    // We only handle constant update so far.
    return;
  }
  if (LoadSSInfo.is_cond_access()) {
    // Should only upgrade unconditional stream.
    return;
  }
  if (!LoadSSInfo.is_trip_count_fixed()) {
    // Should only upgrade load stream with known trip count.
    return;
  }
  LLVM_DEBUG(llvm::errs() << "Upgrade LoadStream " << LoadSS->formatName()
                          << " to UpdateStream.\n");
  LoadSSInfo.set_has_upgraded_to_update(true);
  auto StoreSS = LoadSS->UpdateStream;
  /**
   * Two things we have to do:
   * 1. Since we only target constant update, we have to add one more input to
   *    the stream to have the update value. Done in
   *    generateMemStreamConfiguration.
   * 2. Add the cloned StoreInst to PendingRemovedInsts. Done here.
   */
  auto StoreInst = StoreSS->Inst;
  auto ClonedStoreInst = this->getClonedValue(StoreInst);

  this->PendingRemovedInsts.insert(ClonedStoreInst);
}

void StreamExecutionTransformer::mergePredicatedStreams(
    StreamRegionAnalyzer *Analyzer, Stream *LoadStream) {
  auto LoadSS = LoadStream->SStream;
  auto ProcessPredSS = [this, Analyzer, LoadStream](StaticStream *PredSS,
                                                    bool PredTrue) -> void {
    auto PredInst = PredSS->Inst;
    auto PredStream = Analyzer->getChosenStreamByInst(PredInst);
    if (!PredStream || PredStream->SStream != PredSS) {
      // Somehow this predicated stream is not chosen. Ignore it.
      return;
    }
    if (llvm::isa<llvm::StoreInst>(PredInst)) {
      this->mergePredicatedStore(Analyzer, LoadStream, PredStream, PredTrue);
    }
  };
  for (auto PredSS : LoadSS->PredicatedTrueStreams) {
    ProcessPredSS(PredSS, true);
  }
  for (auto PredSS : LoadSS->PredicatedFalseStreams) {
    ProcessPredSS(PredSS, false);
  }
}

void StreamExecutionTransformer::mergePredicatedStore(
    StreamRegionAnalyzer *Analyzer, Stream *LoadStream, Stream *PredStoreStream,
    bool PredTrue) {
  if (!StreamPassMergePredicatedStore) {
    // This feature is disabled.
    return;
  }
  if (LoadStream->getChosenBaseStepRootStreams() !=
      LoadStream->getChosenBaseStreams()) {
    // This is an indirect stream, we only merge iff.
    // 0. The flag is set.
    // 1. They exactly same base streams.
    // 2. The predicted stream has only one base stream, which is the
    // LoadStream.
    if (!StreamPassMergeIndPredicatedStore) {
      return;
    }
    if (LoadStream->getChosenBaseStreams() !=
        PredStoreStream->getChosenBaseStreams()) {
      return;
    }
  }
  if (LoadStream->getSingleChosenStepRootStream() !=
      PredStoreStream->getSingleChosenStepRootStream()) {
    // They have different step root stream.
    return;
  }
  auto LoadSS = LoadStream->SStream;
  auto StoreSS = PredStoreStream->SStream;
  assert(LoadSS->ConfigureLoop == StoreSS->ConfigureLoop &&
         "Predicated streams should have same configure loop.");
  auto ConfigureLoop = LoadSS->ConfigureLoop;
  auto StoreInst = const_cast<llvm::Instruction *>(StoreSS->Inst);
  auto StoreValue = StoreInst->getOperand(0);
  auto SE = this->CachedLI->getScalarEvolution(StoreInst->getFunction());
  auto StoreValueSCEV = SE->getSCEV(StoreValue);
  if (!SE->isLoopInvariant(StoreValueSCEV, ConfigureLoop)) {
    // So far we only handle constant store.
    return;
  }
  // Decided to merge.
  LLVM_DEBUG(llvm::dbgs() << "Merge Predicated Store " << StoreSS->formatName()
                          << " -> " << LoadSS->formatName() << '\n');
  StoreSS->StaticStreamInfo.set_is_merged_predicated_stream(true);
  auto ProtoEntry = LoadSS->StaticStreamInfo.add_merged_predicated_streams();
  ProtoEntry->mutable_id()->set_id(StoreSS->StreamId);
  ProtoEntry->mutable_id()->set_name(StoreSS->formatName());
  ProtoEntry->set_pred_true(PredTrue);
  // Add the store to pending remove.
  auto ClonedStoreInst = this->getClonedValue(StoreInst);
  this->PendingRemovedInsts.insert(ClonedStoreInst);
}

void StreamExecutionTransformer::mergeLoadStoreDG(
    StreamRegionAnalyzer *Analyzer, Stream *StoreStream) {
  auto StoreSS = StoreStream->SStream;
  if (!StoreSS->StoreDG) {
    // No StoreDG to be merged.
    return;
  }
  /**
   * We merge this store into the Loads iff.
   * 1. All the load input streams are chosen.
   */
  for (auto LoadSS : StoreSS->LoadStoreBaseStreams) {
    auto ChosenLoadS = Analyzer->getChosenStreamByInst(LoadSS->Inst);
    if (!ChosenLoadS || ChosenLoadS->SStream != LoadSS) {
      // Not chosen or chosen at different configure loop level.
      return;
    }
  }
  // We can merge these two.
  for (auto LoadSS : StoreSS->LoadStoreBaseStreams) {
    auto LoadProto =
        LoadSS->StaticStreamInfo.add_merged_load_store_dep_streams();
    LoadProto->set_id(StoreSS->StreamId);
    LoadProto->set_name(StoreSS->formatName());
    auto StoreProto =
        StoreSS->StaticStreamInfo.add_merged_load_store_base_streams();
    StoreProto->set_id(LoadSS->StreamId);
    StoreProto->set_name(LoadSS->formatName());
  }
  // Finally, remove the store.
  auto ClonedStoreInst = this->getClonedValue(StoreSS->Inst);
  this->PendingRemovedInsts.insert(ClonedStoreInst);
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
    auto Func = Inst->getFunction();
    LLVM_DEBUG(llvm::dbgs() << "Remove inst " << Func->getName() << " "
                            << Inst->getName() << "\n");
    Inst->eraseFromParent();
    if (llvm::verifyFunction(*Func, &llvm::dbgs())) {
      llvm::errs() << "Function broken after removing PendingRemovedInsts.";
      Func->print(llvm::dbgs());
      assert(false && "Function broken.");
    }
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
    bool Changed = false;
    auto TTI = this->ClonedCachedLI->getTargetTransformInfo(&ClonedFunc);
    do {
      Changed = TransformUtils::eliminateDeadCode(
          ClonedFunc, this->ClonedCachedLI->getTargetLibraryInfo());
      Changed |= TransformUtils::simplifyCFG(
          ClonedFunc, this->ClonedCachedLI->getTargetLibraryInfo(), TTI);
    } while (Changed);
  }
  /**
   * The last thing is to remove any StreamLoad that has no user instruction.
   * Since we mark StreamLoad with side effect, so it would not be removed by
   * previous dead code elimination.
   * A stream is marked without core user if:
   * 1. The StreamLoad is removed.
   * 2. All the dependent and back-dependent streams do not have core user.
   * This is to make sure that a[b[i]] are correctly marked as we need b[] to
   * compute a[] in the SE, even if the core has no usage.
   * TODO: Handle pointer chasing pattern.
   */
  std::queue<Stream *> NoCoreUserStreams;
  for (auto &StreamInstPair : this->StreamToStreamLoadInstMap) {
    auto S = StreamInstPair.first;
    auto StreamLoadInst = StreamInstPair.second;
    if (StreamLoadInst->use_empty()) {
      StreamLoadInst->eraseFromParent();
      StreamInstPair.second = nullptr;
      if (S->getChosenDependentStreams().empty()) {
        // This is a seed for no core user stream.
        NoCoreUserStreams.emplace(S);
      }
    }
  }
  while (!NoCoreUserStreams.empty()) {
    auto S = NoCoreUserStreams.front();
    NoCoreUserStreams.pop();
    // We enforce that all BackIVDependentStreams has no core user.
    bool HasCoreUserForBackIVDepS = false;
    for (auto BackIVDepS : S->getChosenBackIVDependentStreams()) {
      if (!BackIVDepS->SStream->StaticStreamInfo.no_core_user()) {
        // The only case when this is true is the BackIVDepS is reduction and
        // can be completely removed.
        HasCoreUserForBackIVDepS = true;
        continue;
      }
    }
    if (HasCoreUserForBackIVDepS) {
      // We cannot remove this.
      continue;
    }

    auto SS = S->SStream;
    SS->StaticStreamInfo.set_no_core_user(true);

    // Propagate the signal back to base stream.
    for (auto *BaseS : S->getChosenBaseStreams()) {
      // So far only look at LoadStream.
      if (BaseS->SStream->Inst->getOpcode() != llvm::Instruction::Load) {
        continue;
      }
      auto BaseStreamLoadInst = this->StreamToStreamLoadInstMap.at(BaseS);
      if (BaseStreamLoadInst) {
        // The StreamLoad has not been removed.
        continue;
      }
      // Check for any dependent stream.
      for (auto *BaseDepS : BaseS->getChosenDependentStreams()) {
        if (!BaseDepS->SStream->StaticStreamInfo.no_core_user()) {
          // Still some core user from dependent streams.
          continue;
        }
      }
      // Passed all checks, add to the queue.
      NoCoreUserStreams.emplace(BaseS);
    }
  }
  this->StreamToStreamLoadInstMap.clear();
  assert(!llvm::verifyModule(*this->ClonedModule) &&
         "Module broken after clean up.");
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

  auto PHINodeSCEV = SS->SE->getSCEV(PHINode);
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
  } else if (SS->StaticStreamInfo.val_pattern() ==
             ::LLVM::TDG::StreamValuePattern::REDUCTION) {
    // Handle reduction stream.
    // Set the IV pattern to reduction, and the stream will fill in AddrFunc
    // info.
    ProtoConfiguration->set_val_pattern(
        ::LLVM::TDG::StreamValuePattern::REDUCTION);
    // Add the input value.
    int NumInitialValues = 0;
    for (int Idx = 0, NumIncoming = PHINode->getNumIncomingValues();
         Idx < NumIncoming; ++Idx) {
      auto IncomingBB = PHINode->getIncomingBlock(Idx);
      auto IncomingValue = PHINode->getIncomingValue(Idx);
      if (!SS->ConfigureLoop->contains(IncomingBB)) {
        NumInitialValues++;
        ClonedInputValues.push_back(IncomingValue);
      }
    }
    assert(NumInitialValues == 1 &&
           "Multiple initial values for reduction stream.");
    // Any additional input values from the reduction function.
    for (auto Input : S->getReduceFuncInputValues()) {
      ClonedInputValues.push_back(this->getClonedValue(Input));
    }
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
                   << S->formatName() << " root " << StepRootS->formatName()
                   << '\n';
      assert(false);
    }
  }

  assert(S->SStream->Type == StaticStream::TypeT::MEM && "Must be MEM stream.");
  auto SS = static_cast<const StaticMemStream *>(S->SStream);

  this->handleExtraInputValue(S, ClonedInputValues);

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
    // Handle the addr func input values.
    for (auto Input : S->getAddrFuncInputValues()) {
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
        this->addStreamInputSCEV(StrideSCEV, true /* Signed */, InsertBefore,
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
      this->addStreamInputSCEV(ZeroStrideSCEV, true /* Signed */, InsertBefore,
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
      this->addStreamInputSCEV(BackEdgeTakenSCEV, false /* Signed */,
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
  this->addStreamInputSCEV(CurrentSCEV, false /*Signed */, InsertBefore,
                           ClonedSEExpander, ClonedInputValues,
                           ProtoConfiguration);
}

void StreamExecutionTransformer::handleExtraInputValue(
    Stream *S, InputValueVec &ClonedInputValues) {
  auto SS = S->SStream;
  /**
   * If this is a update stream, add one more input at ClonedInputValues[0].
   */
  if (SS->StaticStreamInfo.has_upgraded_to_update()) {
    auto StoreSS = SS->UpdateStream;
    auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(StoreSS->Inst);
    assert(StoreInst && "Invalid UpdateStream.");
    auto StoreValue = StoreInst->getOperand(0);
    auto ClonedStoreValue = this->getClonedValue(StoreValue);
    this->addStreamInputValue(
        ClonedStoreValue, false /* Signed */, ClonedInputValues,
        SS->StaticStreamInfo.mutable_const_update_param());
  }

  /**
   * If this has merged predicate stream, handle inputs from pred func.
   */
  if (SS->StaticStreamInfo.merged_predicated_streams_size()) {
    for (auto Input : S->getPredFuncInputValues()) {
      ClonedInputValues.push_back(this->getClonedValue(Input));
    }
  }

  /**
   * If this is a merged store stream, add one more input for the store value.
   */
  if (SS->StaticStreamInfo.is_merged_predicated_stream()) {
    auto Inst = SS->Inst;
    if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(Inst)) {
      // Add one more input for the store value. It should be loop invariant.
      auto StoreValue = StoreInst->getOperand(0);
      auto ClonedStoreValue = this->getClonedValue(StoreValue);
      this->addStreamInputValue(
          ClonedStoreValue, false /* Signed */, ClonedInputValues,
          SS->StaticStreamInfo.mutable_const_update_param());
    }
  }
}

void StreamExecutionTransformer::addStreamInputSCEV(
    const llvm::SCEV *ClonedSCEV, bool Signed, llvm::Instruction *InsertBefore,
    llvm::SCEVExpander *ClonedSCEVExpander, InputValueVec &ClonedInputValues,
    LLVM::TDG::IVPattern *ProtoConfiguration) {
  auto ProtoInput = ProtoConfiguration->add_params();
  auto ClonedInputValue =
      ClonedSCEVExpander->expandCodeFor(ClonedSCEV, nullptr, InsertBefore);
  this->addStreamInputValue(ClonedInputValue, Signed, ClonedInputValues,
                            ProtoInput);
  return;
}

void StreamExecutionTransformer::addStreamInputValue(
    const llvm::Value *ClonedValue, bool Signed,
    InputValueVec &ClonedInputValues, ProtoStreamParam *ProtoParam) {
  if (auto ConstValue = llvm::dyn_cast<llvm::ConstantInt>(ClonedValue)) {
    // This is constant.
    ProtoParam->set_is_static(true);
    if (Signed) {
      ProtoParam->set_value(ConstValue->getSExtValue());
    } else {
      ProtoParam->set_value(ConstValue->getZExtValue());
    }
  } else {
    // Runtime input.
    ProtoParam->set_is_static(false);
    ClonedInputValues.push_back(const_cast<llvm::Value *>(ClonedValue));
  }
  return;
}

#undef DEBUG_TYPE