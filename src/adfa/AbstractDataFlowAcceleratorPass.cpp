/**
 * This file implements an non-speculative abstract datagraph accelerator for
 * inner most loops. Details:
 * 1. Convert the control dependence (postdominance frontier) to data
 * dependence.
 * 2. The original single instruction stream is splited into two streams:
 * the GPP stream and the accelerator (data flow) stream. The GPP stream
 * contains special configure and start instruction to interface with the
 * accelerator, and the accelerator stream contains end token to indicate it has
 * reached the end of one invoke and switch back to GPP.
 */

#include "LoopUnroller.h"
#include "PostDominanceFrontier.h"
#include "Replay.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#define DEBUG_TYPE "AbstractDataFlowAcceleratorPass"
namespace {

/**
 * This class representing an on-building data flow.
 * Currently it is basically used to resolve control dependence.
 * For register and memory dependence, I just leave them there and let the
 * simulator explicitly ignores those dependent instructions from outside.
 */
class DynamicDataFlow {
public:
  DynamicDataFlow();

  /**
   * Add one dynamic instruction to the data flow.
   * This instruction must be alive in the data graph.
   * If DynamicInst is nullptr, we will put an end to the dataflow.
   */
  void addDynamicInst(DynamicInstruction *DynamicInst);

  void configure(llvm::Loop *_Loop, llvm::LoopInfo *LI,
                 const PostDominanceFrontier *_PDF, DataGraph *_DG,
                 TDGSerializer *_Serializer, CachedLoopUnroller *_CachedLU,
                 llvm::ScalarEvolution *_SE);
  void start();

  llvm::Loop *Loop;
  llvm::LoopInfo *LI;
  const PostDominanceFrontier *PDF;
  DataGraph *DG;
  TDGSerializer *Serializer;
  CachedLoopUnroller *CachedLU;
  llvm::ScalarEvolution *SE;

private:
  /**
   * An entry contains the dynamic id and the age.
   * This is used to find the oldest real control dependence.
   */
  using Age = uint64_t;
  using DynamicEntry = std::pair<DynamicInstruction::DynamicId, Age>;
  std::unordered_map<llvm::Instruction *, DynamicEntry> StaticToDynamicMap;
  Age CurrentAge = 0;

  std::list<DynamicInstruction *> Buffer;

  LoopIterCounter InnerMostLoopCounter;

  const int UnrollWidth = 4;

  void fixCtrDependence(DynamicInstruction *DynamicInst);
};

class AbstractDataFlowAcceleratorPass : public ReplayTrace {
public:
  static char ID;
  AbstractDataFlowAcceleratorPass() : ReplayTrace(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    ReplayTrace::getAnalysisUsage(Info);
    Info.addRequired<llvm::PostDominatorTreeWrapperPass>();
  }

protected:
  bool initialize(llvm::Module &Module) override {
    // Call the base initialization.
    bool Ret = ReplayTrace::initialize(Module);
    this->DataFlowSerializer = new TDGSerializer("abs-data-flow");
    this->CachedPDF = new CachedPostDominanceFrontier(
        [this](llvm::Function &Func) -> llvm::PostDominatorTree & {
          return this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(Func)
              .getPostDomTree();
        });
    this->CachedLU = new CachedLoopUnroller();
    // Reset other variables.
    this->State = SEARCHING;
    // A window of one thousand instructions is profitable for dataflow?
    this->BufferThreshold = 50;

    this->MemorizedLoopDataflow.clear();

    // Initialize some basic static information.
    LLVM::TDG::StaticInformation StaticInfo;
    StaticInfo.set_module(Module.getName().str());
    this->DataFlowSerializer->serializeStaticInfo(StaticInfo);
    return Ret;
  }

  bool finalize(llvm::Module &Module) override {

    this->MemorizedLoopDataflow.clear();

    // Release the data flow serializer.
    delete this->DataFlowSerializer;
    this->DataFlowSerializer = nullptr;
    delete this->CachedLU;
    this->CachedLU = nullptr;
    delete this->CachedPDF;
    this->CachedPDF = nullptr;
    return ReplayTrace::finalize(Module);
  }

  enum {
    SEARCHING,
    BUFFERING,
    DATAFLOW,
  } State;

  /**
   * Another serializer to the data flow stream
   */
  TDGSerializer *DataFlowSerializer;

  CachedPostDominanceFrontier *CachedPDF;
  CachedLoopUnroller *CachedLU;

  // Memorize the result of isDataflow.
  std::unordered_map<llvm::Loop *, bool> MemorizedLoopDataflow;

  size_t BufferThreshold;

  /**
   * Represent the current data flow configured in the accelerator.
   * If the accelerator has not been configured, it is nullptr.
   */
  DynamicDataFlow CurrentConfiguredDataFlow;

  void transform() override;

  /**
   * Processed the buffered instructions.
   * Returns whether we have started the dataflow execution.
   * No matter what, the buffered instructions will be committed (except the
   * last one).
   */
  bool processBuffer(llvm::Loop *Loop, uint32_t LoopIter);

  /**
   * Serialize to the normal instruction stream.
   */
  void serializeInstStream(DynamicInstruction *DynamicInst);

  /**
   * Serialized the instruction to the data flow stream.
   */
  void serializeDataFlow(DynamicInstruction *DynamicInst);

  /**
   * This function checks if a loop can be represented as dataflow.
   * 1. It has to be inner most loop.
   * 2. It does not call other functions, (except some supported math
   * functions).
   */
  bool isLoopDataFlow(llvm::Loop *Loop);
}; // namespace

void AbstractDataFlowAcceleratorPass::transform() {

  /**
   * A state machine.
   * SEARCHING:
   *    Initial state. Searching for the start of an inner most loop
   * iteration. If encounter an inner most loop iteration, switch to
   * buffering. Otherwise, commit this instruction.
   *
   * BUFFERING:
   *    Buffer until some number of multiple of iterations and the number of
   * instructions reaches a threshold (longer enough to amortize the overhead of
   * switching to dataflow?).
   *    If it ever reaches this long, we look into the buffered instructions and
   * check some characteristics, e.g. control divergence. If it seems
   * profitable, we will insert configure instruction into the stream and
   * serialize the datagraph to another file for simplicity. And switch to
   * DATAFLOW state.
   *    If not, we just fall back to SEARCHING.
   *
   * DATAFLOW:
   *    In this state, it means we have already configured the dataflow
   * accelerator and we just keep serializing the datagraph to another file
   * until we breaks out the current loop.
   *    Depending on the new breaking instruction, we will either switch to
   * SEARCHING or BUFFERING for the next candidate.
   */
  this->State = SEARCHING;
  uint64_t Count = 0;

  // Varaibles valid only for BUFFERING state.
  llvm::Loop *CurrentLoop;
  uint32_t LoopIter;

  DEBUG(llvm::errs() << "ADFA: start.\n");

  while (true) {

    auto NewDynamicInst = this->Trace->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    llvm::Loop *NewLoop = nullptr;
    // If we are at the header of some candidate loop, can be the same loop or
    // some other loop.
    bool IsAtHeaderOfCandidate = false;

    if (NewDynamicInst != this->Trace->DynamicInstructionList.end()) {
      // This is a new instruction.
      // DEBUG(llvm::errs() << "Loaded new instruction.\n");
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instructions.");

      auto LI = this->CachedLI->getLoopInfo(NewStaticInst->getFunction());
      NewLoop = LI->getLoopFor(NewStaticInst->getParent());

      if (NewLoop != nullptr) {
        if (this->isLoopDataFlow(NewLoop)) {
          // This loop is possible for dataflow.
          // DEBUG(llvm::errs()
          //       << "Check if we are at header of " << printLoop(NewLoop)
          //       << " header " << NewLoop->getHeader()->getName() << " inst "
          //       << NewStaticInst->getName() << " bb "
          //       << NewStaticInst->getParent()->getName() << ".\n");
          // DEBUG(llvm::errs()
          //       << "First non-phi "
          //       << NewStaticInst->getParent()->getFirstNonPHI()->getName()
          //       << ".\n");

          IsAtHeaderOfCandidate =
              LoopUtils::isStaticInstLoopHead(NewLoop, NewStaticInst);
          // DEBUG(llvm::errs() << "Check if we are at header: Done.\n");
        } else {
          // This is not a candidate.
          // DEBUG(llvm::errs() << "Getted loop is not a candidate.\n");
          NewLoop = nullptr;
        }
      }

      // if (IsAtHeaderOfCandidate) {
      //   DEBUG(llvm::errs() << "Hitting header of new candidate loop "
      //                      << printLoop(NewLoop) << "\n");
      // }

    } else {
      // Commit any remaining buffered instructions when hitting the end.
      if (this->State == DATAFLOW) {
        // Simply send the end token to the data flow.
        this->CurrentConfiguredDataFlow.addDynamicInst(nullptr);
        break;
      }
      // After these point, it can only be SEARCHING or BUFFERING.
      // Either case, we can simply commit all of them.
      while (!this->Trace->DynamicInstructionList.empty()) {
        this->serializeInstStream(this->Trace->DynamicInstructionList.front());
        this->Trace->commitOneDynamicInst();
      }
      break;
    }
    // State machine.
    switch (this->State) {
    case SEARCHING: {

      assert(this->Trace->DynamicInstructionList.size() <= 2 &&
             "For searching state, there should be at most 2 dynamic "
             "instructions in the buffer.");

      if (this->Trace->DynamicInstructionList.size() == 2) {
        // We can commit the previous one.
        // DEBUG(llvm::errs() << "ADFA: SEARCHING Serialize.\n");
        this->serializeInstStream(this->Trace->DynamicInstructionList.front());
        this->Trace->commitOneDynamicInst();

        // DEBUG(llvm::errs() << "ADFA: SEARCHING Serialize: Done.\n");
      }

      // If we are at the head of some candidate loop, switch to BUFFERING.
      if (IsAtHeaderOfCandidate) {
        this->State = BUFFERING;
        LoopIter = 0;
        CurrentLoop = NewLoop;
        // DEBUG(llvm::errs() << "ADFA: SEARCHING -> BUFFERING.\n");
      }

      break;
    }
    case DATAFLOW: {

      assert(this->Trace->DynamicInstructionList.size() >= 2 &&
             "For dataflow state, there should be at least 2 dynamic "
             "instructions in the buffer.");

      // Add the previous inst to data flow.
      if (this->Trace->DynamicInstructionList.size() >= 2) {
        auto Iter = this->Trace->DynamicInstructionList.rbegin();
        ++Iter;
        this->CurrentConfiguredDataFlow.addDynamicInst(*Iter);
      }

      if (!CurrentLoop->contains(NewLoop)) {
        // We are outside of the current loop.
        this->CurrentConfiguredDataFlow.addDynamicInst(nullptr);
        if (IsAtHeaderOfCandidate) {
          // We are at the header of some new candidate loop, switch to
          // BUFFERING.
          LoopIter = 0;
          CurrentLoop = NewLoop;
          this->State = BUFFERING;
          // DEBUG(llvm::errs() << "ADFA: DATAFLOW -> BUFFERING.\n");
        } else {
          // We are back to search state.
          this->State = SEARCHING;
          // DEBUG(llvm::errs() << "ADFA: DATAFLOW -> SEARCHING.\n");
        }
      }

      break;
    }
    case BUFFERING: {

      /**
       * Update the LoopIter.
       * 1. If we are inside other loops, increase iter count.
       * 2. If we are in the same loop, but hitting header, increase iter
       * count.
       */
      bool IsAtBoundary = false;
      if (!CurrentLoop->contains(NewLoop)) {
        // We are out of the current loop.
        LoopIter++;
        IsAtBoundary = true;
      } else {
        // We are still in the same loop.
        if (CurrentLoop == NewLoop && IsAtHeaderOfCandidate) {
          // We are back at the header of the current loop.
          LoopIter++;
          IsAtBoundary = true;
        }
      }

      if (!IsAtBoundary) {
        // Keep buffering if we are not at boundary.
        break;
      }

      /**
       * Process the buffer if we buffered enough number of instructions.
       * Be careful to only process the buffer at boundary (one iter ends).
       * This will commit the buffer EXCEPT THE LAST INSTRUCTION.
       */
      bool DataFlowStarted = false;
      if (this->Trace->DynamicInstructionList.size() > this->BufferThreshold) {
        // DEBUG(llvm::errs() << "ADFA: Processing buffer.\n");
        DataFlowStarted = this->processBuffer(CurrentLoop, LoopIter);
        // Clear the loop iter.
        LoopIter = 0;
      }

      /**
       * Determine next state.
       * 1. If we are staying in the same loop,
       *  1.1 If the data flow has already started, then switch to DATAFLOW.
       *  2.2 Otherwise, keep buffering. (Not sure about this).
       * 2. If we are outside the current loop,
       *  2.1 If we are at the header of a new candidate, keep buffering the new
       * loop.
       *  2.2. Otherwise, go back to search. In case 2, we should also
       * commit remaining buffered iters in case the buffered number of
       * instructions is not big enough.
       */
      if (!CurrentLoop->contains(NewLoop)) {
        // Case 2
        if (IsAtHeaderOfCandidate) {
          // Case 2.1.
          // Update the loop and loop iter.
          LoopIter = 0;
          CurrentLoop = NewLoop;
        } else {
          // Case 2.2.
          // DEBUG(llvm::errs() << "ADFA: BUFFERING -> SEARCHING.\n");
          this->State = SEARCHING;
        }

        if (DataFlowStarted) {
          // We are out of the current loop, end the dataflow.
          this->CurrentConfiguredDataFlow.addDynamicInst(nullptr);
          assert(this->Trace->DynamicInstructionList.size() == 1 &&
                 "Data flow ended with remaining instructions in the buffer.");
        } else {
          // Commit remaining insts if we haven't start the dataflow.
          while (this->Trace->DynamicInstructionList.size() > 1) {
            this->serializeInstStream(
                this->Trace->DynamicInstructionList.front());
            this->Trace->commitOneDynamicInst();
          }
        }

      } else {
        // Case 1.
        if (DataFlowStarted) {
          // DEBUG(llvm::errs() << "ADFA: BUFFERING -> DATAFLOW.\n");
          this->State = DATAFLOW;
        }
      }

      break;
    }
    default: {
      llvm_unreachable("ADFA: Invalid machine state.");
      break;
    }
    }
  }

  DEBUG(llvm::errs() << "ADFA end.\n");
}

void AbstractDataFlowAcceleratorPass::serializeInstStream(
    DynamicInstruction *DynamicInst) {
  this->Serializer->serialize(DynamicInst, this->Trace);
}

void AbstractDataFlowAcceleratorPass::serializeDataFlow(
    DynamicInstruction *DynamicInst) {
  this->DataFlowSerializer->serialize(DynamicInst, this->Trace);
}

/**
 * I know this this confusing, but this is not an real instruction, but just
 * tell the accelerator that it has reached the end of this invoke.
 */
class AbsDataFlowEndToken : public DynamicInstruction {
public:
  AbsDataFlowEndToken() {}
  std::string getOpName() const override { return "df-end"; }
  // No customized fields for AbsDataFlowEndToken.
};

/**
 * This instruction will configure the dataflow accelerator.
 * Involves some overhead.
 */
class AbsDataFlowConfigInst : public DynamicInstruction {
public:
  AbsDataFlowConfigInst() {}
  std::string getOpName() const override { return "df-config"; }
  // There should be some customized fields in the future.
  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override {
    // Calling mutable_adfa_config should set this field.
    auto ConfigExtra = ProtobufEntry->mutable_adfa_config();
    assert(ProtobufEntry->has_adfa_config() &&
           "The protobuf entry should have adfa config extra struct.");
    ConfigExtra->set_data_flow("abs-data-flow");
  }
};

/**
 * This instruction will kick the accelerator to start working.
 */
class AbsDataFlowStartInst : public DynamicInstruction {
public:
  AbsDataFlowStartInst() {}
  std::string getOpName() const override { return "df-start"; }
};

bool AbstractDataFlowAcceleratorPass::processBuffer(llvm::Loop *Loop,
                                                    uint32_t LoopIter) {

  // Simply serialize to data flow stream for now.
  if (true) {

    /**
     * Note that all the config/start/end insts will be handled as serialization
     * points in the datagraph simulator, so no need to add dependence
     * information.
     */
    // Check if we need to configure the accelerator.
    if (Loop != this->CurrentConfiguredDataFlow.Loop) {
      AbsDataFlowConfigInst ConfigInst;
      this->serializeInstStream(&ConfigInst);
      DEBUG(llvm::errs() << "ADFA: configure the accelerator to loop "
                         << printLoop(Loop) << '\n');
      auto Func = Loop->getHeader()->getParent();
      // Configure the dynamic data flow.
      this->CurrentConfiguredDataFlow.configure(
          Loop, this->CachedLI->getLoopInfo(Func),
          this->CachedPDF->getPostDominanceFrontier(Func), this->Trace,
          this->DataFlowSerializer, this->CachedLU,
          this->CachedLI->getScalarEvolution(Func));
    }

    // Serialize the start inst.
    {

      DEBUG(llvm::errs() << "ADFA: start the accelerator to loop "
                         << printLoop(Loop) << '\n');
      AbsDataFlowStartInst StartInst;
      this->serializeInstStream(&StartInst);
      // Start the dynamic data flow.
      this->CurrentConfiguredDataFlow.start();
    }

    // Add the instruction to the dataflow buffer.
    auto End = this->Trace->DynamicInstructionList.end();
    --End;
    auto Iter = this->Trace->DynamicInstructionList.begin();
    while (Iter != End) {
      auto Next = Iter;
      ++Next;
      auto DynamicInst = *Iter;
      this->CurrentConfiguredDataFlow.addDynamicInst(DynamicInst);
      Iter = Next;
    }
    return true;
  }

  while (this->Trace->DynamicInstructionList.size() > 1) {
    this->serializeInstStream(this->Trace->DynamicInstructionList.front());
    this->Trace->commitOneDynamicInst();
  }

  return false;
}

bool AbstractDataFlowAcceleratorPass::isLoopDataFlow(llvm::Loop *Loop) {

  assert(Loop != nullptr && "Loop should not be nullptr.");

  auto Iter = this->MemorizedLoopDataflow.find(Loop);
  if (Iter != this->MemorizedLoopDataflow.end()) {
    return Iter->second;
  }

  // We allow nested loops.
  bool IsDataFlow = true;
  if (!LoopUtils::isLoopContinuous(Loop)) {
    // DEBUG(llvm::errs() << "Loop " << printLoop(Loop)
    //                    << " is not dataflow as it is not continuous.\n");

    IsDataFlow = false;
  }

  // Done: this loop can be represented as data flow.
  // DEBUG(llvm::errs() << "isLoopDataFlow returned true.\n");
  this->MemorizedLoopDataflow.emplace(Loop, IsDataFlow);
  return IsDataFlow;
}

DynamicDataFlow::DynamicDataFlow()
    : Loop(nullptr), PDF(nullptr), CurrentAge(0) {}

void DynamicDataFlow::configure(llvm::Loop *_Loop, llvm::LoopInfo *_LI,
                                const PostDominanceFrontier *_PDF,
                                DataGraph *_DG, TDGSerializer *_Serializer,
                                CachedLoopUnroller *_CachedLU,
                                llvm::ScalarEvolution *_SE) {
  this->Loop = _Loop;
  this->LI = _LI;
  this->PDF = _PDF;
  this->DG = _DG;
  this->Serializer = _Serializer;
  this->CachedLU = _CachedLU;
  this->SE = _SE;
  this->CurrentAge = 0;
  DEBUG(llvm::errs() << "Config the dynamic data flow for loop "
                     << this->Loop->getName() << '\n');
  DEBUG(llvm::errs() << "Post Dominance Frontier:\n");
  DEBUG(this->PDF->print(llvm::errs()));
}

void DynamicDataFlow::fixCtrDependence(DynamicInstruction *DynamicInst) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr &&
         "DynamicDataFlow cannot fix dependence for non llvm instructions.");

  // Only worried about control dependence.
  auto &CtrDeps = this->DG->CtrDeps.at(DynamicInst->Id);
  CtrDeps.clear();

  auto CtrDepId = DynamicInstruction::InvalidId;
  uint64_t CtrDepAge;
  for (auto CtrBB : this->PDF->getFrontier(StaticInst->getParent())) {
    // Get the terminator of the CtrBB.
    auto CtrInst = CtrBB->getTerminator();
    auto Iter = this->StaticToDynamicMap.find(CtrInst);
    if (Iter == this->StaticToDynamicMap.end()) {
      continue;
    }
    if (CtrDepId == DynamicInstruction::InvalidId) {
      // The first control dependence we have met.
      CtrDepAge = Iter->second.second;
      CtrDepId = Iter->second.first;
    } else if (CtrDepAge < Iter->second.second) {
      // Later control dependence.
      CtrDepAge = Iter->second.second;
      CtrDepId = Iter->second.first;
    }
  }
  // Update the ctr dep.
  if (CtrDepId != DynamicInstruction::InvalidId) {
    CtrDeps.insert(CtrDepId);
  }
}

void DynamicDataFlow::addDynamicInst(DynamicInstruction *DynamicInst) {

  if (DynamicInst != nullptr) {
    DEBUG(llvm::errs() << "ADFA: Add dynamic instruction to the data flow "
                       << DynamicInst->getOpName() << '\n');
  } else {
    DEBUG(llvm::errs() << "ADFA: End the data flow.\n");
  }

  {
    // Update the loop counter.
    // Notice that LoopIterCounter will always consider a nullptr as outside
    // loop, so we do not have to specially handle the nullptr.
    int IterCount;
    llvm::Instruction *StaticInst = nullptr;
    if (DynamicInst != nullptr) {
      StaticInst = DynamicInst->getStaticInstruction();
      assert(StaticInst != nullptr &&
             "DynamicDataFlow cannot handle non llvm instructions.");
    }
    if (this->InnerMostLoopCounter.isConfigured()) {
      auto Status = this->InnerMostLoopCounter.count(StaticInst, IterCount);

      DEBUG(llvm::errs() << "We are in the inner most loop "
                         << printLoop(this->InnerMostLoopCounter.getLoop())
                         << " with iter " << IterCount << '\n');
      bool Unrolled = false;
      if ((IterCount > 0) && (IterCount % this->UnrollWidth) == 0 &&
          (Status == LoopIterCounter::Status::OUT ||
           Status == LoopIterCounter::Status::NEW_ITER)) {
        // We have reached the unroll width.
        // Unroll.
        DEBUG(llvm::errs() << "We are unrolling.\n");
        Unrolled = true;
        auto LU = this->CachedLU->getUnroller(
            this->InnerMostLoopCounter.getLoop(), this->SE);
        if (LU->canUnroll()) {
          LU->unroll(this->DG, this->Buffer.begin(), this->Buffer.end());
        }
      }
      // Serialize and commit all buffered instruction.
      if (Status == LoopIterCounter::Status::OUT || Unrolled) {
        while (this->Buffer.size() > 0) {
          auto SerializedInst = this->Buffer.front();
          DEBUG(llvm::errs() << "ADFA::DF: Serializing "
                             << SerializedInst->getOpName() << '\n');
          this->Serializer->serialize(SerializedInst, this->DG);
          this->DG->commitOneDynamicInst();
          this->Buffer.pop_front();
        }
      }
    }
  }

  if (DynamicInst != nullptr) {
    // Add the new instruction to our buffer.
    this->Buffer.push_back(DynamicInst);

    // First fix the control dependence.
    this->fixCtrDependence(DynamicInst);

    // Update our static to dynamic map.
    auto StaticInst = DynamicInst->getStaticInstruction();
    assert(StaticInst != nullptr &&
           "DynamicDataFlow cannot handle non llvm instructions.");

    auto Iter = this->StaticToDynamicMap.find(StaticInst);
    if (Iter == this->StaticToDynamicMap.end()) {
      this->StaticToDynamicMap.emplace(
          std::piecewise_construct, std::forward_as_tuple(StaticInst),
          std::forward_as_tuple(DynamicInst->Id, this->CurrentAge++));
    } else {
      Iter->second.first = DynamicInst->Id;
      Iter->second.second = this->CurrentAge++;
    }

    auto NewLoop = this->LI->getLoopFor(StaticInst->getParent());
    if (NewLoop->empty()) {
      if (NewLoop != this->InnerMostLoopCounter.getLoop()) {
        assert(this->Buffer.front() == DynamicInst &&
               "The new loop header is still not the header.");
        // A new inner most loop, we reset the iter counter.
        this->InnerMostLoopCounter.configure(NewLoop);
        // Kick start the counter.
        int DummyIter;
        this->InnerMostLoopCounter.count(StaticInst, DummyIter);
      } else {
        // Keep buffering for possible future unrolling.
      }
    } else {
      // As we will only unroll the inner most loop, we do not buffer for these
      // instructions not within the inner most loop.
      // At this point, it should have reached the head.
      this->InnerMostLoopCounter.reset();
      assert(this->Buffer.front() == DynamicInst &&
             "This is still not the header.");
      DEBUG(llvm::errs() << "ADFA::DF: do not buffer non-inner-most loop inst "
                         << DynamicInst->getOpName() << '\n');
      this->Serializer->serialize(DynamicInst, this->DG);
      // Release the instructions.
      this->DG->commitOneDynamicInst();
      this->Buffer.pop_front();
    }
  } else {
    // We need to end the dataflow.
    assert(this->Buffer.empty() && "The dataflow is stll not empty.");
    DEBUG(llvm::errs() << "ADFA::DF: Serialize end token to data flow.");
    auto EndToken = new AbsDataFlowEndToken();
    this->Serializer->serialize(EndToken, this->DG);
    delete EndToken;
  }
}

void DynamicDataFlow::start() {
  this->StaticToDynamicMap.clear();
  this->CurrentAge = 0;
  this->InnerMostLoopCounter.reset();
  assert(this->Buffer.empty() &&
         "Some instructions are remained in the buffer.");
}

#undef DEBUG_TYPE

} // namespace

char AbstractDataFlowAcceleratorPass::ID = 0;
static llvm::RegisterPass<AbstractDataFlowAcceleratorPass>
    X("abs-data-flow-acc-pass", "Abstract datagraph accelerator transform pass",
      false, false);
