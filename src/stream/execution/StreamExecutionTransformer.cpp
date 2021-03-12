#include "StreamExecutionTransformer.h"
#include "StaticNestStreamBuilder.h"

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
#define DEBUG_FUNC "WritePixelCachePixels"

StreamExecutionTransformer::StreamExecutionTransformer(
    llvm::Module *_Module,
    const std::unordered_set<llvm::Function *> &_ROIFunctions,
    CachedLoopInfo *_CachedLI, std::string _OutputExtraFolderPath,
    bool _TransformTextMode,
    const std::vector<StaticStreamRegionAnalyzer *> &Analyzers)
    : Module(_Module), ROIFunctions(_ROIFunctions), CachedLI(_CachedLI),
      OutputExtraFolderPath(_OutputExtraFolderPath),
      TransformTextMode(_TransformTextMode),
      NestStreamBuilder(new StaticNestStreamBuilder(this)) {
  // Copy the module.
  LLVM_DEBUG(llvm::dbgs() << "Clone the module.\n");
  this->ClonedModule = llvm::CloneModule(*(this->Module), this->ClonedValueMap);
  LLVM_DEBUG(if (auto Func = this->ClonedModule->getFunction(DEBUG_FUNC)) {
    llvm::dbgs() << "--------------- Just Cloned --------------\n";
    Func->print(llvm::dbgs());
  });
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
  LLVM_DEBUG(llvm::dbgs() << "Clean the module.\n");
  this->cleanClonedModule();

  // Replace with our StreamMemIntrinsic.
  this->replaceWithStreamMemIntrinsic();

  // Generate the module.
  LLVM_DEBUG(llvm::dbgs() << "Write the module.\n");
  this->writeModule();
  this->writeAllConfiguredRegions();
  // * Must be called after writeModule().
  this->writeAllTransformedFunctions();
}

StreamExecutionTransformer::~StreamExecutionTransformer() {}

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
    StaticStreamRegionAnalyzer *Analyzer) {
  auto TopLoop = Analyzer->getTopLoop();
  LLVM_DEBUG(llvm::dbgs() << "Transform stream region "
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
       * So far UserDefinedMemStream is treated as a special load.
       */
      if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
        this->transformLoadInst(Analyzer, CallInst);
      }

      /**
       * Replace use of store with StreamStore.
       */
      else if (auto StoreInst = llvm::dyn_cast<llvm::StoreInst>(Inst)) {
        this->transformStoreInst(Analyzer, StoreInst);
      }

      else if (auto AtomicRMWInst = llvm::dyn_cast<llvm::AtomicRMWInst>(Inst)) {
        this->transformAtomicRMWInst(Analyzer, AtomicRMWInst);
      }

      else if (auto CmpXchg = llvm::dyn_cast<llvm::AtomicCmpXchgInst>(Inst)) {
        this->transformAtomicCmpXchgInst(Analyzer, CmpXchg);
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
    Analyzer->coalesceStreamsAtLoop(CurrentLoop);
  }

  /**
   * After we inserted all the stream configure instructions,
   * we try to nest streams.
   */
  this->NestStreamBuilder->buildNestStreams(Analyzer);

  // Insert address computation function in the cloned module.
  Analyzer->insertAddrFuncInModule(this->ClonedModule);
  Analyzer->insertPredicateFuncInModule(this->ClonedModule);
}

void StreamExecutionTransformer::configureStreamsAtLoop(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Loop *Loop) {
  const auto &ConfigureInfo = Analyzer->getConfigureLoopInfo(Loop);
  if (ConfigureInfo.getSortedStreams().empty()) {
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

  LLVM_DEBUG(llvm::dbgs() << "Configure loop " << LoopUtils::getLoopId(Loop)
                          << '\n');
  auto ConfigIdxValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()),
      ConfigIdx, false);
  this->insertStreamConfigAtLoop(Analyzer, Loop, ConfigIdxValue);
  this->insertStreamEndAtLoop(Analyzer, Loop, ConfigIdxValue);
}

void StreamExecutionTransformer::insertStreamConfigAtLoop(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Loop *Loop,
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
  this->LoopToClonedConfigureBBMap.emplace(Loop, ClonedPreheader);
  // Simply insert one stream configure instruction before the terminator.
  auto ClonedPreheaderTerminator = ClonedPreheader->getTerminator();
  llvm::IRBuilder<> Builder(ClonedPreheaderTerminator);
  std::array<llvm::Value *, 1> StreamConfigArgs{ConfigIdxValue};
  Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_config),
      StreamConfigArgs);

  for (auto S : ConfigureInfo.getSortedStreams()) {

    LLVM_DEBUG(if (ClonedPreheader->getParent()->getName() == DEBUG_FUNC) {
      llvm::dbgs() << "--------------- Before Config " << S->formatName()
                   << " --------------\n";
      ClonedPreheader->getParent()->print(llvm::dbgs());
    });

    InputValueVec ClonedInputValues;
    switch (S->Type) {
    case StaticStream::TypeT::IV:
      this->generateIVStreamConfiguration(S, ClonedPreheaderTerminator,
                                          ClonedInputValues);
      break;
    case StaticStream::TypeT::MEM:
      this->generateMemStreamConfiguration(S, ClonedPreheaderTerminator,
                                           ClonedInputValues);
      break;
    case StaticStream::TypeT::USER:
      this->generateUserMemStreamConfiguration(S, ClonedPreheaderTerminator,
                                               ClonedInputValues);
      break;
    default:
      llvm_unreachable("Invalid StaticStream Type.");
    }
    for (auto ClonedInput : ClonedInputValues) {
      // This is a live in?
      LLVM_DEBUG(llvm::dbgs()
                 << "Insert StreamInput for Stream " << S->formatName()
                 << " Input " << ClonedInput->getName() << '\n');
      this->addStreamInput(Builder, S->getRegionStreamId(), ClonedInput);
    }
  }

  // Insert the StreamReady instruction.
  Builder.CreateCall(llvm::Intrinsic::getDeclaration(
      this->ClonedModule.get(), llvm::Intrinsic::ID::ssp_stream_ready));
}

void StreamExecutionTransformer::insertStreamEndAtLoop(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Loop *Loop,
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
  auto &InsertedStreamEnds =
      this->LoopToClonedEndInstsMap
          .emplace(std::piecewise_construct, std::forward_as_tuple(Loop),
                   std::forward_as_tuple())
          .first->second;
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
      auto StreamEndInst = Builder.CreateCall(
          llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                          llvm::Intrinsic::ID::ssp_stream_end),
          StreamEndArgs);
      InsertedStreamEnds.push_back(StreamEndInst);
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

  /**
   * Handle UserDefinedMemStream's EndInst.
   * So far I just remove it as streams are still bound to a ConfigureLoop.
   */
  for (auto S : ConfigureInfo.getSortedStreams()) {
    if (S->Type == StaticStream::TypeT::USER) {
      auto UserS = static_cast<UserDefinedMemStream *>(S);
      auto EndInst = UserS->getEndInst();
      auto ClonedEndInst = this->getClonedValue(EndInst);
      this->PendingRemovedInsts.insert(ClonedEndInst);
    }
  }
}

void StreamExecutionTransformer::insertStreamReduceAtLoop(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Loop *Loop,
    StaticStream *ReduceStream) {

  const auto &SSInfo = ReduceStream->StaticStreamInfo;
  if (SSInfo.val_pattern() != ::LLVM::TDG::StreamValuePattern::REDUCTION) {
    return;
  }

  auto PHINode = llvm::dyn_cast<llvm::PHINode>(ReduceStream->Inst);
  assert(PHINode && "Reduction stream should be PHINode.");
  auto ExitValue = ReduceStream->ReduceDG->getSingleResultValue();
  auto ExitInst = llvm::dyn_cast<llvm::Instruction>(ExitValue);
  assert(ExitInst &&
         "ExitValue should be an instruction for reduction stream.");
  // Add a final StreamLoad.
  auto ClonedLoop = this->getOrCreateLoopInClonedModule(Loop);

  /**
   * Let's only handle one exit block so far.
   * Try to replace out-of-loop user.
   */
  auto ClonedExitBB = ClonedLoop->getExitBlock();
  if (!ClonedExitBB) {
    return;
  }
  auto ClonedExitInst = this->getClonedValue(ExitInst);
  auto ClonedReducedValue = this->addStreamLoadOrAtomic(
      ReduceStream, ClonedExitInst->getType(), ClonedExitBB->getFirstNonPHI());
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
  for (auto ComputeInst : ReduceStream->ReduceDG->getComputeInsts()) {
    ClonedS.insert(this->getClonedValue(ComputeInst));
  }
  bool IsComplete = true;
  for (auto ClonedInst : ClonedS) {
    for (auto User : ClonedInst->users()) {
      if (auto UserInst = llvm::dyn_cast<llvm::Instruction>(User)) {
        if (!ClonedS.count(UserInst) &&
            !this->PendingRemovedInsts.count(UserInst)) {
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
    ReduceStream->StaticStreamInfo.set_no_core_user(true);
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

  LLVM_DEBUG(llvm::dbgs() << "Failed to find preheader, creating one.\n");
  auto ClonedFunc = ClonedLoop->getHeader()->getParent();
  auto ClonedLI = this->ClonedCachedLI->getLoopInfo(ClonedFunc);
  auto ClonedDT = this->ClonedCachedLI->getDominatorTree(ClonedFunc);
  // We pass LI and DT so that they are updated.
  auto NewPreheader = llvm::InsertPreheaderForLoop(ClonedLoop, ClonedDT,
                                                   ClonedLI, nullptr, false);

  return NewPreheader;
}

void StreamExecutionTransformer::transformLoadInst(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Instruction *Inst) {
  auto S = Analyzer->getChosenStreamByInst(Inst);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  /**
   * 1. Insert a StreamLoad.
   * 2. Replace the use of original load to StreamLoad.
   * 3. Leave the original load there (not deleted).
   */
  auto ClonedInst = this->getClonedValue(Inst);
  auto StreamLoadInst = this->addStreamLoadOrAtomic(
      S, ClonedInst->getType(), ClonedInst, &ClonedInst->getDebugLoc());
  ClonedInst->replaceAllUsesWith(StreamLoadInst);

  // Insert the load into the pending removed set.
  this->PendingRemovedInsts.insert(ClonedInst);

  this->upgradeLoadToUpdateStream(Analyzer, S);
  this->mergePredicatedStreams(Analyzer, S);
}

llvm::Value *StreamExecutionTransformer::addStreamLoadOrAtomic(
    StaticStream *S, llvm::Type *LoadType,
    llvm::Instruction *ClonedInsertBefore, const llvm::DebugLoc *DebugLoc,
    bool isAtomic) {
  /**
   * Insert a StreamLoad for S.
   */

  // Here we should RegionStreamId to fit in immediate field.
  auto StreamId = S->getRegionStreamId();
  assert(StreamId >= 0 && StreamId < 128 &&
         "Illegal RegionStreamId for StreamLoad.");
  auto StreamIdValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()), StreamId,
      false);
  std::array<llvm::Value *, 1> StreamLoadArgs{StreamIdValue};

  auto StreamLoadType = LoadType;
  bool NeedTruncate = false;
  bool NeedIntToPtr = false;
  bool NeedBitcast = false;
  /**
   * ! It takes an effort to promote the loaded value to 64-bit in RV64,
   * ! so as a temporary fix, I truncate it here.
   */
  auto NumBits = S->DataLayout->getTypeStoreSizeInBits(LoadType);
  if (auto IntType = llvm::dyn_cast<llvm::IntegerType>(LoadType)) {
    if (IntType->getBitWidth() < 64) {
      StreamLoadType =
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
      NeedTruncate = true;
    } else if (IntType->getBitWidth() > 64) {
      assert(false && "Cannot handle load type larger than 64 bit.");
    }
  } else if (auto PtrType = llvm::dyn_cast<llvm::PointerType>(LoadType)) {
    StreamLoadType =
        llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
    NeedIntToPtr = true;
  } else if (auto VecType = llvm::dyn_cast<llvm::VectorType>(LoadType)) {
    auto ElementType = VecType->getVectorElementType();
    if (ElementType->isIntegerTy() || ElementType->isFloatingPointTy()) {
      // Make sure these are correct type.
      auto CastedNumFP64 = 0;
      if (NumBits == 64 || NumBits == 128 || NumBits == 256 || NumBits == 512) {
        CastedNumFP64 = NumBits / 64;
      } else {
        llvm::errs() << "Invalid vector type bits " << NumBits << " Type:\n";
        LoadType->print(llvm::errs());
        llvm_unreachable("Invalid number of bits for vector.");
      }
      if (CastedNumFP64 == 1) {
        StreamLoadType =
            llvm::Type::getDoubleTy(this->ClonedModule->getContext());
      } else {
        StreamLoadType = llvm::VectorType::get(
            llvm::Type::getDoubleTy(this->ClonedModule->getContext()),
            CastedNumFP64);
      }
      NeedBitcast = true;
    } else {
      llvm::errs() << "Invalid vector element type.";
      assert(false);
    }
  }

  std::array<llvm::Type *, 1> StreamLoadTypes{StreamLoadType};
  llvm::IRBuilder<> Builder(ClonedInsertBefore);

  auto IntrinsicID = isAtomic ? llvm::Intrinsic::ID::ssp_stream_atomic
                              : llvm::Intrinsic::ID::ssp_stream_load;

  auto StreamLoadInst = Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(), IntrinsicID,
                                      StreamLoadTypes),
      StreamLoadArgs);
  if (DebugLoc) {
    StreamLoadInst->setDebugLoc(llvm::DebugLoc(DebugLoc->get()));
  }
  if (!isAtomic) {
    this->StreamToStreamLoadInstMap.emplace(S, StreamLoadInst);
  }
  if (NeedTruncate) {
    auto TruncateInst = Builder.CreateTrunc(StreamLoadInst, LoadType);
    return TruncateInst;
  } else if (NeedIntToPtr) {
    auto IntToPtrInst = Builder.CreateIntToPtr(StreamLoadInst, LoadType);
    return IntToPtrInst;
  } else if (NeedBitcast) {
    auto BitcastInst = Builder.CreateBitCast(
        StreamLoadInst, LoadType, "ssp.load.bitcast." + S->Inst->getName());
    return BitcastInst;
  } else {
    return StreamLoadInst;
  }
}

void StreamExecutionTransformer::addStreamStore(
    StaticStream *S, llvm::Instruction *ClonedInsertBefore,
    const llvm::DebugLoc *DebugLoc) {
  if (!S->isChosen()) {
    llvm::errs() << "Try to add StreamStore for unchosen stream "
                 << S->formatName() << '\n';
    assert(false);
  }
  auto StreamId = S->getRegionStreamId();
  auto StreamIdValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()), StreamId,
      false);
  std::array<llvm::Value *, 1> Args{StreamIdValue};

  llvm::IRBuilder<> Builder(ClonedInsertBefore);
  auto StreamStoreInst = Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_store),
      Args);
  if (DebugLoc) {
    StreamStoreInst->setDebugLoc(llvm::DebugLoc(DebugLoc->get()));
  }
}

void StreamExecutionTransformer::addStreamInput(llvm::IRBuilder<> &Builder,
                                                int StreamId,
                                                llvm::Value *ClonedInputValue) {
  auto ClonedInputType = ClonedInputValue->getType();
  /**
   * Since StreamInput instruction does not care about data type,
   * For scalar type, we always cast to int64.
   * For vector type, we always cast to v8f64/v4f64/v2f64;
   */
  if (auto VecType = llvm::dyn_cast<llvm::VectorType>(ClonedInputType)) {
    auto StoreBits = this->ClonedDataLayout->getTypeStoreSizeInBits(VecType);
    if (StoreBits == 512 || StoreBits == 256 || StoreBits == 128) {
      auto NumElements = StoreBits / sizeof(double) / 8;
      ClonedInputType = llvm::VectorType::get(
          llvm::Type::getDoubleTy(this->ClonedModule->getContext()),
          NumElements);
      ClonedInputValue = Builder.CreateBitCast(
          ClonedInputValue, ClonedInputType, "ssp.input.bitcast");
    } else {
      llvm::errs() << "Unsupported vector input type: ";
      VecType->print(llvm::errs());
      llvm_unreachable("Illegal vector stream input.");
    }
  } else {
    if (ClonedInputType->isFloatTy()) {
      // Float -> Int32
      ClonedInputType =
          llvm::IntegerType::getInt32Ty(this->ClonedModule->getContext());
      ClonedInputValue = Builder.CreateBitCast(
          ClonedInputValue, ClonedInputType, "ssp.input.bitcast");
    } else if (ClonedInputType->isDoubleTy()) {
      // Double -> Int64
      ClonedInputType =
          llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
      ClonedInputValue = Builder.CreateBitCast(
          ClonedInputValue, ClonedInputType, "ssp.input.bitcast");
    }

    // Final extension: Int8/Int16/Int32 -> Int64.
    if (auto IntType = llvm::dyn_cast<llvm::IntegerType>(ClonedInputType)) {
      if (IntType->getBitWidth() < 64) {
        ClonedInputType =
            llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext());
        ClonedInputValue =
            Builder.CreateZExt(ClonedInputValue, ClonedInputType);
      } else if (IntType->getBitWidth() > 64) {
        assert(false && "Cannot handle input type larger than 64 bit.");
      }
    }
  }

  assert(StreamId >= 0 && StreamId < 128 &&
         "Illegal RegionStreamId for StreamInput.");
  auto StreamIdValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->ClonedModule->getContext()), StreamId,
      false);
  std::array<llvm::Value *, 2> StreamInputArgs{StreamIdValue, ClonedInputValue};
  std::array<llvm::Type *, 1> StreamInputType{ClonedInputType};
  auto StreamInputInst = Builder.CreateCall(
      llvm::Intrinsic::getDeclaration(this->ClonedModule.get(),
                                      llvm::Intrinsic::ID::ssp_stream_input,
                                      StreamInputType),
      StreamInputArgs);
}

void StreamExecutionTransformer::transformStoreInst(
    StaticStreamRegionAnalyzer *Analyzer, llvm::StoreInst *StoreInst) {
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

  this->handleValueDG(Analyzer, S);
}

void StreamExecutionTransformer::transformAtomicRMWInst(
    StaticStreamRegionAnalyzer *Analyzer, llvm::AtomicRMWInst *AtomicRMWInst) {
  auto S = Analyzer->getChosenStreamByInst(AtomicRMWInst);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  /**
   * AtomicRMW stream is treated similar to store stream.
   */

  // Handle ValueDG.
  this->handleValueDG(Analyzer, S);
}

void StreamExecutionTransformer::transformAtomicCmpXchgInst(
    StaticStreamRegionAnalyzer *Analyzer, llvm::AtomicCmpXchgInst *CmpXchg) {
  auto S = Analyzer->getChosenStreamByInst(CmpXchg);
  if (S == nullptr) {
    // This is not a chosen stream.
    return;
  }

  /**
   * AtomicRMW stream is treated similar to store stream.
   */

  // Handle ValueDG.
  this->handleValueDG(Analyzer, S);
}

void StreamExecutionTransformer::transformStepInst(
    StaticStreamRegionAnalyzer *Analyzer, llvm::Instruction *StepInst) {
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
  /**
   * If this is a CallInst to ssp_step, we can mark it pending removed.
   */
  if (Utils::isCallOrInvokeInst(StepInst)) {
    auto Callee = Utils::getCalledValue(StepInst);
    if (Callee->getName() == "ssp_step") {
      auto ClonedStepInst = this->getClonedValue(StepInst);
      this->PendingRemovedInsts.insert(ClonedStepInst);
    }
  }
}

void StreamExecutionTransformer::upgradeLoadToUpdateStream(
    StaticStreamRegionAnalyzer *Analyzer, StaticStream *LoadSS) {
  auto &LoadSSInfo = LoadSS->StaticStreamInfo;
  auto StoreSS = LoadSS->UpdateStream;
  if (!StoreSS) {
    // There is no StoreSS.
    return;
  }
  /**
   * We simply call handleValueDG on the StoreSS, which will take care
   * of setting fields if this is an UpdateStream.
   */
  this->handleValueDG(Analyzer, StoreSS);
  if (!LoadSS->StaticStreamInfo.compute_info().enabled_store_func()) {
    return;
  }
  LLVM_DEBUG(llvm::dbgs() << "Upgrade LoadStream " << LoadSS->formatName()
                          << " to UpdateStream.\n");
}

void StreamExecutionTransformer::mergePredicatedStreams(
    StaticStreamRegionAnalyzer *Analyzer, StaticStream *LoadSS) {
  auto ProcessPredSS = [this, Analyzer, LoadSS](StaticStream *PredSS,
                                                bool PredTrue) -> void {
    auto PredInst = PredSS->Inst;
    auto PredStream = Analyzer->getChosenStreamByInst(PredInst);
    if (!PredStream || PredStream != PredSS) {
      // Somehow this predicated stream is not chosen. Ignore it.
      return;
    }
    if (llvm::isa<llvm::StoreInst>(PredInst)) {
      this->mergePredicatedStore(Analyzer, LoadSS, PredStream, PredTrue);
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
    StaticStreamRegionAnalyzer *Analyzer, StaticStream *LoadSS,
    StaticStream *StoreSS, bool PredTrue) {
  if (!StreamPassMergePredicatedStore) {
    // This feature is disabled.
    return;
  }
  if (LoadSS->ChosenBaseStepRootStreams != LoadSS->ChosenBaseStreams) {
    // This is an indirect stream, we only merge iff.
    // 0. The flag is set.
    // 1. They have exactly same base streams.
    // 2. The predicted stream has only one base stream, which is the
    // LoadStream.
    if (!StreamPassMergeIndPredicatedStore) {
      return;
    }
    if (LoadSS->ChosenBaseStreams != StoreSS->ChosenBaseStreams) {
      return;
    }
  }
  if (LoadSS->ChosenBaseStepRootStreams != StoreSS->ChosenBaseStepRootStreams ||
      LoadSS->ChosenBaseStepRootStreams.size() != 1) {
    // They have different step root stream.
    return;
  }
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

void StreamExecutionTransformer::handleValueDG(
    StaticStreamRegionAnalyzer *Analyzer, StaticStream *SS) {
  if (!SS->ValueDG) {
    // No ValueDG to be merged.
    return;
  }
  /**
   * We enable the ValueDG as:
   * 1. Check that all the load input streams are chosen.
   * 2. Remember the value dependence.
   * 3. Enable the store func in the store stream.
   */
  for (auto LoadSS : SS->LoadStoreBaseStreams) {
    auto ChosenLoadS = Analyzer->getChosenStreamByInst(LoadSS->Inst);
    if (!ChosenLoadS || ChosenLoadS != LoadSS) {
      // Not chosen or chosen at different configure loop level.
      return;
    }
  }
  // Remember the value dependence (in the LoadSS if this is UpdateSS).
  auto SSProtoComputeInfo = SS->StaticStreamInfo.mutable_compute_info();
  auto UpdateProtoComputeInfo =
      SS->UpdateStream
          ? SS->UpdateStream->StaticStreamInfo.mutable_compute_info()
          : nullptr;
  for (auto LoadSS : SS->LoadStoreBaseStreams) {
    auto LoadProto = LoadSS->StaticStreamInfo.mutable_compute_info()
                         ->add_value_dep_streams();
    if (SS->UpdateStream) {
      // Also remember the information in the base load stream.
      LoadProto->set_id(SS->UpdateStream->StreamId);
      LoadProto->set_name(SS->UpdateStream->formatName());
      auto SSProto = UpdateProtoComputeInfo->add_value_base_streams();
      SSProto->set_id(LoadSS->StreamId);
      SSProto->set_name(LoadSS->formatName());
    } else {
      LoadProto->set_id(SS->StreamId);
      LoadProto->set_name(SS->formatName());
      auto SSProto =
          SS->StaticStreamInfo.mutable_compute_info()->add_value_base_streams();
      SSProto->set_id(LoadSS->StreamId);
      SSProto->set_name(LoadSS->formatName());
    }
  }
  // Enable the store func.
  if (SS->UpdateStream) {
    UpdateProtoComputeInfo->set_enabled_store_func(true);
    SS->fillProtobufStoreFuncInfoImpl(
        UpdateProtoComputeInfo->mutable_store_func_info(), false /* IsLoad */);
  } else {
    SSProtoComputeInfo->set_enabled_store_func(true);
  }

  /**
   * Finally, we transform the program.
   * 1. For store stream, replace the inst with a placeholder StreamStore.
   * 2. For atomic stream, replace the inst with a placehodler StreamAtomic.
   * We cannot use StreamLoad + StreamStore to represent an atomic stream,
   * as that breaks atomicity. However, a StreamAtomic can be broken into
   * a load and a store in microops.
   */
  auto ClonedSSInst = this->getClonedValue(SS->Inst);
  this->PendingRemovedInsts.insert(ClonedSSInst);

  // Consider FusedLoadOps.
  auto ClonedFinalValueInst = ClonedSSInst;
  for (auto FusedOp : SS->FusedLoadOps) {
    auto ClonedFusedOp = this->getClonedValue(FusedOp);
    this->PendingRemovedInsts.insert(ClonedFusedOp);
    ClonedFinalValueInst = ClonedFusedOp;
  }

  if (llvm::isa<llvm::StoreInst>(SS->Inst)) {
    if (SS->UpdateStream) {
      this->addStreamStore(SS->UpdateStream, ClonedSSInst,
                           &ClonedSSInst->getDebugLoc());
    } else {
      this->addStreamStore(SS, ClonedSSInst, &ClonedSSInst->getDebugLoc());
    }
  } else {
    // This is an atomic stream.
    if (ClonedFinalValueInst->use_empty()) {
      // Mark this stream without core user.
      SS->StaticStreamInfo.set_no_core_user(true);
    }
    auto StreamAtomicInst = this->addStreamLoadOrAtomic(
        SS, ClonedFinalValueInst->getType(), ClonedFinalValueInst,
        &ClonedFinalValueInst->getDebugLoc(), true /* isAtomic */);
    ClonedFinalValueInst->replaceAllUsesWith(StreamAtomicInst);
  }
}

llvm::Instruction *
StreamExecutionTransformer::findStepPosition(StaticStream *StepStream,
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
  auto Loop = StepStream->InnerMostLoop;
  auto LI = this->CachedLI->getLoopInfo(StepInst->getFunction());
  auto DT = this->CachedLI->getDominatorTree(StepInst->getFunction());
  auto StepBB = StepInst->getParent();
  auto Latch = Loop->getLoopLatch();
  /**
   * So far only consider the case with single latch.
   * Multiple latches is compilcated, if one latch can lead to another latch.
   */
  if (!Latch) {
    LLVM_DEBUG(llvm::dbgs() << "Multiple latches for "
                            << LoopUtils::getLoopId(Loop) << '\n');
  }
  if (DT->dominates(StepBB, Latch)) {
    // Simple case.
    return Latch->getTerminator();
  }

  // Sanity check for all dependent streams.
  std::unordered_set<llvm::BasicBlock *> DependentStreamBBs;
  std::queue<StaticStream *> DependentStreamQueue;
  for (auto S : StepStream->ChosenDependentStreams) {
    DependentStreamQueue.push(S);
  }
  while (!DependentStreamQueue.empty()) {
    auto S = DependentStreamQueue.front();
    DependentStreamQueue.pop();
    auto BB = const_cast<llvm::BasicBlock *>(S->Inst->getParent());
    if (BB == StepBB) {
      // StepBB is fine.
      continue;
    }
    DependentStreamBBs.insert(BB);
    for (auto DepS : S->ChosenDependentStreams) {
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
  // ! Also, we release CachedLoopInfo so that we can safely remove any
  // ! instructions.
  this->ClonedCachedLI->clearAnalysis();

  // Group by functions.
  std::unordered_map<llvm::Function *, std::list<llvm::Instruction *>>
      FuncPendingRemovedInstMap;
  for (auto *Inst : this->PendingRemovedInsts) {
    auto Func = Inst->getFunction();
    FuncPendingRemovedInstMap
        .emplace(std::piecewise_construct, std::forward_as_tuple(Func),
                 std::forward_as_tuple())
        .first->second.push_back(Inst);
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
    auto PendingRemovedInstIter = FuncPendingRemovedInstMap.find(&ClonedFunc);
    if (PendingRemovedInstIter == FuncPendingRemovedInstMap.end()) {
      continue;
    }
    this->removePendingRemovedInstsInFunc(ClonedFunc,
                                          PendingRemovedInstIter->second);
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
  std::queue<StaticStream *> NoCoreUserStreams;
  for (auto &StreamInstPair : this->StreamToStreamLoadInstMap) {
    auto S = StreamInstPair.first;
    auto StreamLoadInst = StreamInstPair.second;
    if (StreamLoadInst->use_empty()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Remove unused StreamLoad "
                 << StreamLoadInst->getFunction()->getName() << ' '
                 << Utils::formatLLVMInstWithoutFunc(StreamLoadInst) << '\n');
      StreamLoadInst->eraseFromParent();
      StreamInstPair.second = nullptr;

      bool allDepSNoCoreUser = true;
      for (auto DepS : S->ChosenDependentStreams) {
        if (!DepS->StaticStreamInfo.no_core_user()) {
          allDepSNoCoreUser = false;
          break;
        }
      }
      if (allDepSNoCoreUser) {
        // This is a seed for no core user stream.
        NoCoreUserStreams.emplace(S);
      }
    }
  }
  while (!NoCoreUserStreams.empty()) {
    auto SS = NoCoreUserStreams.front();
    NoCoreUserStreams.pop();
    // We enforce that all BackIVDependentStreams has no core user.
    bool HasCoreUserForBackIVDepS = false;
    for (auto BackIVDepS : SS->ChosenBackIVDependentStreams) {
      if (!BackIVDepS->StaticStreamInfo.no_core_user()) {
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

    SS->StaticStreamInfo.set_no_core_user(true);

    // Propagate the signal back to base stream.
    for (auto *BaseS : SS->ChosenBaseStreams) {
      // So far only look at LoadStream.
      if (BaseS->Inst->getOpcode() != llvm::Instruction::Load) {
        continue;
      }
      auto BaseStreamLoadInst = this->StreamToStreamLoadInstMap.at(BaseS);
      if (BaseStreamLoadInst) {
        // The StreamLoad has not been removed.
        continue;
      }
      // Check for any dependent stream.
      for (auto *BaseDepS : BaseS->ChosenDependentStreams) {
        if (!BaseDepS->StaticStreamInfo.no_core_user()) {
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

void StreamExecutionTransformer::removePendingRemovedInstsInFunc(
    llvm::Function &ClonedFunc,
    std::list<llvm::Instruction *> &PendingRemovedInsts) {
  LLVM_DEBUG(if (ClonedFunc.getName() == DEBUG_FUNC) {
    llvm::dbgs() << "------------------- Before Removing "
                    "PendingInst ---------------\n";
    ClonedFunc.print(llvm::dbgs());
  });

  /**
   * It is possible that some pending removed instructions still have
   * users, as long as the user is also pending removed. Therefore, we
   * clean it one by one.
   */
  while (true) {
    bool RemovedSomeInsts = false;
    for (auto InstIter = PendingRemovedInsts.begin(),
              InstEnd = PendingRemovedInsts.end();
         InstIter != InstEnd;) {
      auto *Inst = *InstIter;
      /**
       * There are some instructions that may not be eliminated by the DCE,
       * e.g. the replaced store instruction.
       * So we delete them here.
       */
      if (!Inst->use_empty()) {
        ++InstIter;
        continue;
      }
      auto Func = Inst->getFunction();
      LLVM_DEBUG(llvm::dbgs()
                 << "Remove inst " << Func->getName() << " "
                 << Utils::formatLLVMInstWithoutFunc(Inst) << "\n");
      Inst->eraseFromParent();
      if (llvm::verifyFunction(*Func, &llvm::dbgs())) {
        llvm::errs() << "Function broken after removing PendingRemovedInsts.";
        Func->print(llvm::dbgs());
        assert(false && "Function broken.");
      }
      InstIter = PendingRemovedInsts.erase(InstIter);
      RemovedSomeInsts = true;
    }
    if (!RemovedSomeInsts) {
      if (!PendingRemovedInsts.empty()) {
        llvm::errs() << "Some PendingRemovedInsts has use:.\n";
        for (auto *Inst : PendingRemovedInsts) {
          auto Func = Inst->getFunction();
          llvm::errs() << "Cannot remove inst " << Func->getName() << " "
                       << Utils::formatLLVMInstWithoutFunc(Inst) << "\n";
        }
        assert(false && "Can not fully remove PendingRemovedInsts.");
      }
      break;
    }
  }

  bool Changed = false;
  auto TTI = this->ClonedCachedLI->getTargetTransformInfo(&ClonedFunc);
  do {
    LLVM_DEBUG(llvm::dbgs()
                   << "------------------- Before DCE and Simplify CFG "
                      "PendingInst ---------------\n";
               ClonedFunc.print(llvm::dbgs()));
    Changed = TransformUtils::eliminateDeadCode(
        ClonedFunc, this->ClonedCachedLI->getTargetLibraryInfo());
    Changed |= TransformUtils::simplifyCFG(
        ClonedFunc, this->ClonedCachedLI->getTargetLibraryInfo(), TTI);
  } while (Changed);
}

void StreamExecutionTransformer::generateIVStreamConfiguration(
    StaticStream *S, llvm::Instruction *InsertBefore,
    std::vector<llvm::Value *> &ClonedInputValues) {
  assert(S->Type == StaticStream::TypeT::IV && "Must be IV stream.");
  auto SS = static_cast<const StaticIndVarStream *>(S);
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

  const llvm::SCEV *PHINodeSCEV = nullptr;
  const llvm::SCEV *ClonedPHINodeSCEV = nullptr;
  if (SS->SE->isSCEVable(PHINode->getType())) {
    PHINodeSCEV = SS->SE->getSCEV(PHINode);
    ClonedPHINodeSCEV = ClonedSE->getSCEV(ClonedPHINode);
  }
  LLVM_DEBUG({
    llvm::dbgs() << "====== Generate IVStreamConfiguration " << SS->formatName()
                 << " PHINodeSCEV ";
    if (PHINodeSCEV)
      PHINodeSCEV->dump();
    else
      llvm::dbgs() << "None\n";
  });

  if (PHINodeSCEV && llvm::isa<llvm::SCEVAddRecExpr>(PHINodeSCEV)) {
    auto PHINodeAddRecSCEV = llvm::dyn_cast<llvm::SCEVAddRecExpr>(PHINodeSCEV);
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
  } else if (SS->StaticStreamInfo.val_pattern() ==
                 ::LLVM::TDG::StreamValuePattern::LINEAR &&
             SS->StaticStreamInfo.stp_pattern() ==
                 ::LLVM::TDG::StreamStepPattern::CONDITIONAL) {
    llvm::errs() << "Can't handle Conditional Linear IVStream "
                 << S->formatName() << '\n';
    assert(false);
  } else {
    llvm::errs() << "Can't handle IVStream " << S->formatName() << '\n';
    assert(false);
  }
}

void StreamExecutionTransformer::generateMemStreamConfiguration(
    StaticStream *S, llvm::Instruction *InsertBefore,
    InputValueVec &ClonedInputValues) {

  LLVM_DEBUG(llvm::dbgs() << "===== Generate MemStreamConfiguration "
                          << S->formatName() << '\n');

  assert(S->Type == StaticStream::TypeT::MEM && "Must be Mem stream.");
  auto SS = static_cast<const StaticMemStream *>(S);

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

  if (SS->StaticStreamInfo.val_pattern() ==
      ::LLVM::TDG::StreamValuePattern::INDIRECT) {
    // Check if this is indirect stream.
    LLVM_DEBUG(llvm::dbgs() << "This is Indirect MemStream.\n");
    for (auto BaseSS : S->ChosenBaseStreams) {
      if (BaseSS->ConfigureLoop != SS->ConfigureLoop ||
          BaseSS->InnerMostLoop != SS->InnerMostLoop) {
        if (SS->StaticStreamInfo.stp_pattern() !=
                ::LLVM::TDG::StreamStepPattern::UNCONDITIONAL ||
            BaseSS->StaticStreamInfo.stp_pattern() !=
                ::LLVM::TDG::StreamStepPattern::UNCONDITIONAL) {
          llvm::errs() << "Cannot handle indirect streams (mismatch loop, "
                          "conditional step): \n";
          llvm::errs() << "Base: " << BaseSS->formatName() << '\n';
          llvm::errs() << "Dep:  " << SS->formatName() << '\n';
          assert(false && "Mismatch in configure loop for indirect streams.");
        }
      }
    }
    // Set the IV pattern to indirect, and the stream will fill in AddrFunc
    // info.
    ProtoConfiguration->set_val_pattern(
        ::LLVM::TDG::StreamValuePattern::INDIRECT);
    // Handle the addr func input values.
    for (auto Input : SS->getAddrFuncInputValues()) {
      ClonedInputValues.push_back(this->getClonedValue(Input));
    }
    return;
  } else if (SS->StaticStreamInfo.val_pattern() ==
             ::LLVM::TDG::StreamValuePattern::LINEAR) {
    if (SS->StaticStreamInfo.stp_pattern() ==
        ::LLVM::TDG::StreamStepPattern::UNCONDITIONAL) {
      if (!llvm::isa<llvm::SCEVAddRecExpr>(AddrSCEV)) {
        llvm::errs() << "Unconditional LinearMemStream " << SS->formatName()
                     << " should have AddRecSCEV, but got ";
        AddrSCEV->print(llvm::errs());
        llvm::errs() << '\n';
        assert(false && "Unconditional linear stream should have AddRecSCEV.");
      }
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
    } else if (SS->StaticStreamInfo.stp_pattern() ==
               ::LLVM::TDG::StreamStepPattern::CONDITIONAL) {
      llvm::errs() << "Can't handle conditional linear mem stream "
                   << S->formatName() << '\n';
      assert(false);
    }
  }

  llvm::errs() << "Can't handle this stream " << SS->formatName() << '\n';
  llvm::errs() << "AddrSCEV: ";
  AddrSCEV->print(llvm::errs());
  assert(false && "Can't handle this Addr.");
}

void StreamExecutionTransformer::generateUserMemStreamConfiguration(
    StaticStream *S, llvm::Instruction *InsertBefore,
    InputValueVec &ClonedInputValues) {

  LLVM_DEBUG(llvm::dbgs() << "===== Generate UserMemStreamConfiguration "
                          << S->formatName() << '\n');

  assert(S->Type == StaticStream::TypeT::USER && "Must be UserMem stream.");
  auto SS = static_cast<const UserDefinedMemStream *>(S);

  this->handleExtraInputValue(S, ClonedInputValues);

  auto ProtoConfiguration = SS->StaticStreamInfo.mutable_iv_pattern();

  auto ClonedFunction = this->getClonedValue(SS->Inst->getFunction());
  auto ClonedSE = this->ClonedCachedLI->getScalarEvolution(ClonedFunction);
  auto ClonedSEExpander = this->ClonedCachedLI->getSCEVExpander(ClonedFunction);

  if (S->StaticStreamInfo.val_pattern() !=
      ::LLVM::TDG::StreamValuePattern::LINEAR) {
    llvm::errs() << "Non linear UserMemStream " << S->formatName() << '\n';
    assert(false);
  }
  if (S->StaticStreamInfo.stp_pattern() !=
      ::LLVM::TDG::StreamStepPattern::UNCONDITIONAL) {
    llvm::errs() << "Conditional step UserMemStream " << S->formatName()
                 << '\n';
    assert(false);
  }

  auto ClonedConfigureLoop =
      this->getOrCreateLoopInClonedModule(SS->ConfigureLoop);
  auto ClonedInnerMostLoop =
      this->getOrCreateLoopInClonedModule(SS->InnerMostLoop);

  auto ConfigInst = SS->getConfigInst();
  auto ClonedConfigInst = this->getClonedValue(ConfigInst);
  ProtoConfiguration->set_val_pattern(::LLVM::TDG::StreamValuePattern::LINEAR);
  /**
   * Although not a good design, we assume the params are
   * stride, trip_count, stride, ..., start_addr.
   * With signed stride and unsigned trip_count.
   */
  for (auto ArgIdx = 0u, NumArgs = ClonedConfigInst->arg_size();
       ArgIdx < NumArgs; ArgIdx++) {
    auto ClonedInputValue = ClonedConfigInst->getArgOperand(ArgIdx);
    bool Signed = true;
    if (((ArgIdx % 2) == 1) || (ArgIdx + 1 == NumArgs)) {
      Signed = false;
    }
    auto ProtoInput = ProtoConfiguration->add_params();
    this->addStreamInputValue(ClonedInputValue, Signed, ClonedInputValues,
                              ProtoInput);
  }
  // Add the ConfigInst to PendingRemoved instructions.
  this->PendingRemovedInsts.insert(ClonedConfigInst);
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
      llvm::dbgs() << "Peeling LoopHeader " << CurrentLoop->getName() << ' ';
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
        llvm::errs() << "Cannot handle this LoopVariant SCEV:";
        CurrentSCEV->print(llvm::errs());
        llvm::errs() << '\n';
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
    auto TripCountSCEV = LoopUtils::getTripCountSCEV(ClonedSE, CurrentLoop);
    LLVM_DEBUG({
      llvm::errs() << "TripCount ";
      TripCountSCEV->dump();
    });
    if (!llvm::isa<llvm::SCEVCouldNotCompute>(TripCountSCEV) &&
        ClonedSE->isLoopInvariant(TripCountSCEV, ClonedConfigureLoop)) {
      this->addStreamInputSCEV(TripCountSCEV, false /* Signed */, InsertBefore,
                               ClonedSEExpander, ClonedInputValues,
                               ProtoConfiguration);
    } else {
      assert(CurrentLoop == ClonedConfigureLoop &&
             "Need const TripCount for nested loop.");
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
    StaticStream *SS, InputValueVec &ClonedInputValues) {
  /**
   * If this has merged predicate stream, handle inputs from pred func.
   */
  if (SS->StaticStreamInfo.merged_predicated_streams_size()) {
    for (auto Input : SS->getPredFuncInputValues()) {
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

  /**
   * If this stream enabled ValueDG, add more ValueDG inputs.
   */
  if (SS->StaticStreamInfo.compute_info().enabled_store_func()) {
    if (SS->Inst->getOpcode() == llvm::Instruction::Load) {
      // This should be the update stream.
      assert(SS->UpdateStream &&
             "Missing UpdateStream for LoadStream with StoreFunc.");
      for (auto Input : SS->UpdateStream->getStoreFuncInputValues()) {
        ClonedInputValues.push_back(this->getClonedValue(Input));
      }
    } else {
      assert(SS->ValueDG && "No ValueDG for MergedLoadStoreDepStream.");
      for (auto Input : SS->getStoreFuncInputValues()) {
        ClonedInputValues.push_back(this->getClonedValue(Input));
      }
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

void StreamExecutionTransformer::replaceWithStreamMemIntrinsic() {

  bool enableStreamMemIntrinsic = true;
  if (!enableStreamMemIntrinsic) {
    return;
  }

  // Check if we have stream_memset.
  auto ClonedStreamMemsetFunc =
      this->ClonedModule->getFunction("stream_memset");
  auto ClonedStreamMemcpyFunc =
      this->ClonedModule->getFunction("stream_memcpy");
  auto ClonedStreamMemmoveFunc =
      this->ClonedModule->getFunction("stream_memmove");
  if (!ClonedStreamMemsetFunc && !ClonedStreamMemcpyFunc &&
      !ClonedStreamMemmoveFunc) {
    return;
  }
  for (auto Func : this->ROIFunctions) {
    for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
         ++BBIter) {
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
          auto Callee = CallInst->getCalledFunction();
          if (Callee->getIntrinsicID() == llvm::Intrinsic::ID::memset &&
              ClonedStreamMemsetFunc) {
            // Check that the last volatile argument is false.
            auto ClonedCallInst = this->getClonedValue(CallInst);
            auto DestArg = ClonedCallInst->getOperand(0);
            auto ValArg = ClonedCallInst->getOperand(1);
            auto LenArg = ClonedCallInst->getOperand(2);
            auto VolatileArg = ClonedCallInst->getOperand(3);
            if (auto ConstVolatileArg =
                    llvm::dyn_cast<llvm::ConstantInt>(VolatileArg)) {
              if (ConstVolatileArg->isZero()) {
                llvm::IRBuilder<> Builder(ClonedCallInst);
                // We ignore the last volatile arguments.
                std::vector<llvm::Value *> Args{
                    DestArg,
                    ValArg,
                    LenArg,
                };
                Builder.CreateCall(ClonedStreamMemsetFunc, Args);
                ClonedCallInst->eraseFromParent();
              }
            }
          } else if (Callee->getIntrinsicID() == llvm::Intrinsic::ID::memcpy &&
                     ClonedStreamMemcpyFunc) {
            // Check that the last volatile argument is false.
            auto ClonedCallInst = this->getClonedValue(CallInst);
            auto DestArg = ClonedCallInst->getOperand(0);
            auto SrcArg = ClonedCallInst->getOperand(1);
            auto LenArg = ClonedCallInst->getOperand(2);
            auto VolatileArg = ClonedCallInst->getOperand(3);
            if (auto ConstVolatileArg =
                    llvm::dyn_cast<llvm::ConstantInt>(VolatileArg)) {
              if (ConstVolatileArg->isZero()) {
                llvm::IRBuilder<> Builder(ClonedCallInst);
                // We ignore the last volatile arguments.
                std::vector<llvm::Value *> Args{
                    DestArg,
                    SrcArg,
                    LenArg,
                };
                Builder.CreateCall(ClonedStreamMemcpyFunc, Args);
                ClonedCallInst->eraseFromParent();
              }
            }
          } else if (Callee->getIntrinsicID() == llvm::Intrinsic::ID::memmove &&
                     ClonedStreamMemmoveFunc) {
            // Check that the last volatile argument is false.
            auto ClonedCallInst = this->getClonedValue(CallInst);
            auto DestArg = ClonedCallInst->getOperand(0);
            auto SrcArg = ClonedCallInst->getOperand(1);
            auto LenArg = ClonedCallInst->getOperand(2);
            auto VolatileArg = ClonedCallInst->getOperand(3);
            if (auto ConstVolatileArg =
                    llvm::dyn_cast<llvm::ConstantInt>(VolatileArg)) {
              if (ConstVolatileArg->isZero()) {
                llvm::IRBuilder<> Builder(ClonedCallInst);
                // We ignore the last volatile arguments.
                std::vector<llvm::Value *> Args{
                    DestArg,
                    SrcArg,
                    LenArg,
                };
                Builder.CreateCall(ClonedStreamMemmoveFunc, Args);
                ClonedCallInst->eraseFromParent();
              }
            }
          }
        }
      }
    }
  }
}

llvm::BasicBlock *StreamExecutionTransformer::getClonedConfigureBBForLoop(
    const llvm::Loop *Loop) const {
  auto Iter = this->LoopToClonedConfigureBBMap.find(Loop);
  if (Iter == this->LoopToClonedConfigureBBMap.end()) {
    llvm::errs() << "Failed to find ClonedConfigureBB for Loop "
                 << LoopUtils::getLoopId(Loop) << '\n';
    assert(false);
  }
  return Iter->second;
}

const std::vector<llvm::CallInst *> &
StreamExecutionTransformer::getClonedEndInstsForLoop(
    const llvm::Loop *Loop) const {
  auto Iter = this->LoopToClonedEndInstsMap.find(Loop);
  if (Iter == this->LoopToClonedEndInstsMap.end()) {
    llvm::errs() << "Failed to find ClonedEndBB for Loop "
                 << LoopUtils::getLoopId(Loop) << '\n';
    assert(false);
  }
  return Iter->second;
}

#undef DEBUG_TYPE