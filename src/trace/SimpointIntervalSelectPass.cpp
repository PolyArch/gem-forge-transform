#include "SimpointIntervalSelectPass.h"
#include "../ProtobufSerializer.h"
#include "../Utils.h"
#include "ProfileLogger.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

// Reuse the -trace-file Option.
#include "../DataGraph.h"

#define DEBUG_TYPE "SimpointIntervalSelectPass"

SimpointIntervalSelectPass::SimpointIntervalSelectPass()
    : GemForgeBasePass(ID) {}

SimpointIntervalSelectPass::~SimpointIntervalSelectPass() {}

bool SimpointIntervalSelectPass::runOnModule(llvm::Module &Module) {
  this->initialize(Module);

  // Initialize the tree.
  this->CLProfileTree = new CallLoopProfileTree(this->CachedLI);

  // Initialize the reader.
  {
    GzipMultipleProtobufReader Reader(TraceFileName);
    uint64_t UID;
    while (Reader.readVarint64(UID)) {
      auto Inst = Utils::getInstUIDMap().getInst(UID);
      this->CLProfileTree->addInstruction(Inst);
    }
  }

  this->CLProfileTree->aggregateStats();
  this->CLProfileTree->selectEdges();

  std::string TreeFileName = TraceFileName + ".tree.txt";
  std::ofstream OTree(TreeFileName);
  this->CLProfileTree->dump(OTree);
  OTree.close();

  this->generateProfileInterval();
  this->generateEdgeTimeline();

  // Insert the edge mark.
  this->registerEdgeMarkFunc();
  const auto &Edges = this->CLProfileTree->getSelectedEdges();
  for (const auto &E : Edges) {
    this->insertEdgeMark(E);
  }

  delete this->CLProfileTree;
  this->CLProfileTree = nullptr;

  this->finalize(Module);

  return false;
}

void SimpointIntervalSelectPass::generateProfileInterval() {

  const auto &Points = this->CLProfileTree->getSelectedEdgeTimeline();
  GzipMultipleProtobufReader Reader(TraceFileName);
  ProfileLogger Logger;

  // Search for the first interval point.
  uint64_t IntervalLHSInstCount = 0;
  uint64_t IntervalRHSInstCount = 0;
  uint64_t IntervalLHSMarkCount = 0;
  uint64_t IntervalRHSMarkCount = 0;
  auto PointIter = Points.begin();
  auto PointEnd = Points.end();
  while (PointIter != PointEnd) {
    if (PointIter->Selected) {
      IntervalLHSInstCount = PointIter->TraverseInstCount;
      IntervalRHSInstCount = PointIter->TraverseInstCount;
      IntervalLHSMarkCount = PointIter->TraverseMarkCount;
      IntervalRHSMarkCount = PointIter->TraverseMarkCount;
      PointIter++;
      break;
    }
    PointIter++;
  }
  uint64_t UID;
  while (Reader.readVarint64(UID)) {
    auto InstCount = Logger.getCurrentInstCount();
    if (InstCount >= IntervalRHSInstCount) {
      if (IntervalRHSInstCount > IntervalLHSInstCount) {
        // We have a valid interval.
        assert(Logger.getCurrentIntervalLHS() == IntervalLHSInstCount);
        Logger.saveAndRestartInterval(IntervalRHSMarkCount);
      } else {
        // This is the first invalid interval.
        Logger.discardAndRestartInterval(IntervalRHSMarkCount);
      }
      IntervalLHSInstCount = IntervalRHSInstCount;
      IntervalLHSMarkCount = IntervalRHSMarkCount;
      // Search for next RHSInterval.
      while (PointIter != PointEnd) {
        if (PointIter->Selected) {
          IntervalRHSInstCount = PointIter->TraverseInstCount;
          IntervalRHSMarkCount = PointIter->TraverseMarkCount;
          PointIter++;
          break;
        }
        PointIter++;
      }
    }
    auto Inst = Utils::getInstUIDMap().getInst(UID);
    Logger.addInst(Inst->getFunction()->getName().str(),
                   Inst->getParent()->getName().str());
  }
  std::string ProfileFileName = TraceFileName + ".profile";
  Logger.serializeToFile(ProfileFileName);
}

void SimpointIntervalSelectPass::generateEdgeTimeline() {
  const auto &Points = this->CLProfileTree->getSelectedEdgeTimeline();
  std::string TimelineFileName = TraceFileName + ".timeline.txt";
  std::ofstream OTimeline(TimelineFileName);
  for (const auto &P : Points) {
    OTimeline << "# ====== Mark " << P.TraverseMarkCount << " LLVM Inst "
              << P.TraverseInstCount << " Edge " << P.E->SelectedID << '\n';
    OTimeline << "#  " << Utils::formatLLVMInst(P.E->StartInst) << '\n';
    OTimeline << "#  " << Utils::formatLLVMInst(P.E->BridgeInst) << '\n';
    OTimeline << "#  " << Utils::formatLLVMInst(P.E->DestInst) << '\n';
    OTimeline << P.E->SelectedID << '\n';
  }
}

void SimpointIntervalSelectPass::registerEdgeMarkFunc() {
  auto &Context = this->Module->getContext();
  auto VoidTy = llvm::Type::getVoidTy(Context);
  auto Int64Ty = llvm::Type::getInt64Ty(Context);
  std::vector<llvm::Type *> EdgeMarkFuncArgs{
      Int64Ty, // uint64_t mark id
      Int64Ty, // uint64_t thread id
  };
  auto EdgeMarkFuncTy =
      llvm::FunctionType::get(VoidTy, EdgeMarkFuncArgs, false);
  this->EdgeMarkFunc =
      this->Module->getOrInsertFunction("m5_work_mark", EdgeMarkFuncTy);
}

void SimpointIntervalSelectPass::insertEdgeMark(
    const CallLoopProfileTree::EdgePtr &Edge) {
  auto StartInst = Edge->StartInst;
  auto BridgeInst = Edge->BridgeInst;
  auto DestInst = Edge->DestInst;
  auto SelectedID = Edge->SelectedID;
  assert(StartInst);
  assert(BridgeInst);
  assert(DestInst);
  LLVM_DEBUG(llvm::dbgs() << "Insert Edge " << SelectedID << " ====\n  "
                          << Utils::formatLLVMInst(StartInst) << "\n  "
                          << Utils::formatLLVMInst(BridgeInst) << "\n  "
                          << Utils::formatLLVMInst(DestInst) << '\n');
  std::vector<llvm::Value *> EdgeMarkArgs{
      // SelectedID.
      llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(this->Module->getContext()), SelectedID,
          false /* IsSigned */
          ),
      // ThreadID.
      llvm::ConstantInt::get(
          llvm::IntegerType::getInt64Ty(this->Module->getContext()), 0,
          false /* IsSigned */
          ),
  };
  if (Utils::isCallOrInvokeInst(BridgeInst)) {
    if (Utils::getCalledFunction(BridgeInst)) {
      // Insert edge mark before the call.
      llvm::IRBuilder<> Builder(BridgeInst);
      Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
    } else {
      // Check if this indirect call has single destination.
      const CallLoopProfileTree::Node *StartNode =
          this->CLProfileTree->getNode(StartInst);
      const CallLoopProfileTree::Node *DestNode =
          this->CLProfileTree->getNode(DestInst);
      int NumSameBridgeEdges = 0;
      int NumDestInEdges = DestNode->InEdges.size();
      bool HasDifferentCallee = false;
      for (auto &OutE : StartNode->OutEdges) {
        if (OutE->BridgeInst == BridgeInst) {
          NumSameBridgeEdges++;
          if (OutE->DestInst != DestInst) {
            HasDifferentCallee = true;
          }
        }
      }
      if (NumSameBridgeEdges == 1 && !HasDifferentCallee) {
        // This indirect call has same destination, can be treated as
        // direct call.
        llvm::IRBuilder<> Builder(BridgeInst);
        Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
      } else if (NumDestInEdges == 1) {
        // The callee only has one coming edge. We can move the edge to
        // the callee.
        llvm::IRBuilder<> Builder(
            &*BridgeInst->getParent()->getFirstInsertionPt());
        Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
      } else {
        llvm::errs() << "Cannot handle IndirectCall Edge with #"
                     << NumSameBridgeEdges << " Callees "
                     << " and #" << NumDestInEdges << " DestInEdges "
                     << SelectedID << " ====\n  "
                     << Utils::formatLLVMInst(StartInst) << "\n  "
                     << Utils::formatLLVMInst(BridgeInst) << "\n  "
                     << Utils::formatLLVMInst(DestInst) << '\n';
        assert(false && "Cannot handle IndirectCall so far.");
      }
    }
  } else if (auto BridgeBrInst = llvm::dyn_cast<llvm::BranchInst>(BridgeInst)) {
    if (BridgeBrInst->isUnconditional()) {
      // Unconditional branch is simple.
      llvm::IRBuilder<> Builder(BridgeInst);
      Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
    } else {
      auto BridgeBB = BridgeInst->getParent();
      auto DestBB = DestInst->getParent();
      if (DestBB->getSinglePredecessor() == BridgeBB) {
        // If we only have single predecessor, just insert at DestBB.
        llvm::IRBuilder<> Builder(&*DestBB->getFirstInsertionPt());
        Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
      } else {
        // We have to split the edge.
        auto Func = DestBB->getParent();
        auto DT = this->CachedLI->getDominatorTree(Func);
        auto LI = this->CachedLI->getLoopInfo(Func);
        auto EdgeBB = llvm::SplitEdge(BridgeBB, DestBB, DT, LI);
        llvm::IRBuilder<> Builder(&*EdgeBB->getFirstInsertionPt());
        Builder.CreateCall(this->EdgeMarkFunc, EdgeMarkArgs);
      }
    }
  } else {
    llvm::errs() << "Cannot handle Edge " << SelectedID << " ====\n  "
                 << Utils::formatLLVMInst(StartInst) << "\n  "
                 << Utils::formatLLVMInst(BridgeInst) << "\n  "
                 << Utils::formatLLVMInst(DestInst) << '\n';
  }
}

char SimpointIntervalSelectPass::ID = 0;
static llvm::RegisterPass<SimpointIntervalSelectPass>
    R("simpoint-interval", "Select simpoint interval from trace", false, false);