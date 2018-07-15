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

#include "LoopUtils.h"
#include "PostDominanceFrontier.h"
#include "Replay.h"

#include "llvm/Analysis/LoopInfo.h"
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

  void fixDependence(DynamicInstruction *DynamicInst, DataGraph *DG);

  void addDynamicInst(DynamicInstruction *DynamicInst);

  void configure(llvm::Loop *_Loop, const PostDominanceFrontier *_PDF);
  void start();

  llvm::Loop *Loop;
  const PostDominanceFrontier *PDF;

private:
  /**
   * An entry contains the dynamic id and the age.
   * This is used to find the oldest real control dependence.
   */
  using Age = uint64_t;
  using DynamicEntry = std::pair<DynamicInstruction::DynamicId, Age>;
  std::unordered_map<llvm::Instruction *, DynamicEntry> StaticToDynamicMap;
  Age CurrentAge = 0;
};

class AbstractDataFlowAcceleratorPass : public ReplayTrace {
public:
  static char ID;
  AbstractDataFlowAcceleratorPass() : ReplayTrace(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    ReplayTrace::getAnalysisUsage(Info);
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addRequired<llvm::ScalarEvolutionWrapperPass>();
    Info.addRequired<llvm::PostDominatorTreeWrapperPass>();
  }

protected:
  bool initialize(llvm::Module &Module) override {
    this->DataFlowSerializer = new TDGSerializer("abs-data-flow");
    this->CachedLI = new CachedLoopInfo();
    this->CachedPDF = new CachedPostDominanceFrontier(
        [this](llvm::Function &Func) -> llvm::PostDominatorTree & {
          return this->getAnalysis<llvm::PostDominatorTreeWrapperPass>(Func)
              .getPostDomTree();
        });
    // Reset the cached static inner most loop.
    this->CachedStaticInnerMostLoop.clear();
    // Reset other variables.
    this->State = SEARCHING;
    // A window of one thousand instructions is profitable for dataflow?
    this->BufferThreshold = 50;
    // Call the base initialization.
    return ReplayTrace::initialize(Module);
  }

  bool finalize(llvm::Module &Module) override {
    // Release the data flow serializer.
    delete this->DataFlowSerializer;
    this->DataFlowSerializer = nullptr;
    // Remember to release the static inner most loop first.
    for (auto &Entry : this->CachedStaticInnerMostLoop) {
      delete Entry.second;
    }
    this->CachedStaticInnerMostLoop.clear();
    // Release the cached static loops.
    delete this->CachedLI;
    this->CachedLI = nullptr;
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

  CachedLoopInfo *CachedLI;
  CachedPostDominanceFrontier *CachedPDF;

  /**
   * Contains caches StaticInnerMostLoop.
   */
  std::unordered_map<llvm::Loop *, StaticInnerMostLoop *>
      CachedStaticInnerMostLoop;

  size_t BufferThreshold;

  /**
   * Represent the current data flow configured in the accelerator.
   * If the accelerator has not been configured, it is nullptr.
   */
  DynamicDataFlow CurrentConfiguredDataFlow;

  void transform() override;

  /**
   * Processed the buffered instructions.
   * Returns whether we have started the dataflow executioin.
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
   * Serialize an data flow end token to the data flow stream to indicate the
   * boundary of invoke of the accelerator.
   */
  void serializeDataFlowEnd();

  /**
   * This function checks if a loop can be represented as dataflow.
   * 1. It has to be inner most loop.
   * 2. It does not call other functions, (except some supported math
   * functions).
   */
  bool isLoopDataFlow(llvm::Loop *Loop) const;
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
          IsAtHeaderOfCandidate = isStaticInstLoopHead(NewLoop, NewStaticInst);
        } else {
          // This is not a candidate.
          NewLoop = nullptr;
        }
      }

      // if (IsAtHeaderOfCandidate) {
      //   DEBUG(llvm::errs() << "Hitting header of new candidate loop "
      //                      << NewStaticLoop->print() << "\n");
      // }

    } else {
      // Commit any remaining buffered instructions when hitting the end.
      while (!this->Trace->DynamicInstructionList.empty()) {
        if (this->State == DATAFLOW) {
          this->serializeDataFlow(this->Trace->DynamicInstructionList.front());
        } else {
          this->serializeInstStream(
              this->Trace->DynamicInstructionList.front());
        }
        this->Trace->commitOneDynamicInst();
      }
      if (this->State == DATAFLOW) {
        // Remember to serialize the end token.
        this->serializeDataFlowEnd();
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

      assert(this->Trace->DynamicInstructionList.size() <= 2 &&
             "For dataflow state, there should be at most 2 dynamic "
             "instructions in the buffer.");

      if (this->Trace->DynamicInstructionList.size() == 2) {
        // We can commit the previous one.
        this->serializeDataFlow(this->Trace->DynamicInstructionList.front());
        this->Trace->commitOneDynamicInst();
      }

      if (!CurrentLoop->contains(NewLoop)) {
        // We are outside of the current loop.
        this->serializeDataFlowEnd();
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
        // Commit remaining insts.
        while (this->Trace->DynamicInstructionList.size() > 1) {
          assert(
              !DataFlowStarted &&
              "Data flow started with remaining instructions in the buffer.");
          this->serializeInstStream(
              this->Trace->DynamicInstructionList.front());
          this->Trace->commitOneDynamicInst();
        }
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
      } else {
        // Case 1.
        if (DataFlowStarted) {
          // DEBUG(llvm::errs() << "ADFA: BUFFERING -> DATAFLOW.\n");
          this->State = DATAFLOW;
        }
      }

      if (DataFlowStarted && this->State != DATAFLOW) {
        // We started an dataflow, and there is not more comming. Insert the end
        // token.
        this->serializeDataFlowEnd();
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

void AbstractDataFlowAcceleratorPass::serializeDataFlowEnd() {
  // We have to allocate a new instruction to encure the property of unique
  // DynamicId.
  auto EndToken = new AbsDataFlowEndToken();
  this->DataFlowSerializer->serialize(EndToken, this->Trace);
  delete EndToken;
}

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
      // Configure the dynamic data flow.
      this->CurrentConfiguredDataFlow.configure(
          Loop, this->CachedPDF->getPostDominanceFrontier(
                    Loop->getHeader()->getParent()));
    }

    // Serialize the start inst.
    {
      AbsDataFlowStartInst StartInst;
      this->serializeInstStream(&StartInst);
      // Start the dynamic data flow.
      this->CurrentConfiguredDataFlow.start();
    }

    while (this->Trace->DynamicInstructionList.size() > 1) {

      // Fix the dependence for the data flow.
      auto DynamicInst = this->Trace->DynamicInstructionList.front();
      this->CurrentConfiguredDataFlow.fixDependence(DynamicInst, this->Trace);
      this->CurrentConfiguredDataFlow.addDynamicInst(DynamicInst);

      // Serialize to the data flow.
      this->serializeDataFlow(this->Trace->DynamicInstructionList.front());
      this->Trace->commitOneDynamicInst();
    }
    return true;
  }

  while (this->Trace->DynamicInstructionList.size() > 1) {
    this->serializeInstStream(this->Trace->DynamicInstructionList.front());
    this->Trace->commitOneDynamicInst();
  }

  return false;
}

bool AbstractDataFlowAcceleratorPass::isLoopDataFlow(llvm::Loop *Loop) const {

  assert(Loop != nullptr && "Loop should not be nullptr.");

  // We allow nested loops.
  // if (!Loop->empty()) {
  //   // This is not the inner most loop.
  //   DEBUG(llvm::errs() << "AbstractDataFlowAcceleratorPass: Loop "
  //                      << printLoop(Loop)
  //                      << " is statically not dataflow because it is not "
  //                         "inner most loop.\n");
  //   return false;
  // }

  // Check if there is any calls to unsupported function.
  for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
       BBIter != BBEnd; ++BBIter) {
    for (auto InstIter = (*BBIter)->begin(), InstEnd = (*BBIter)->end();
         InstIter != InstEnd; ++InstIter) {
      if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(&*InstIter)) {
        // This is a call inst.
        auto Callee = CallInst->getCalledFunction();
        if (Callee == nullptr) {
          // Indirect call, not vectorizable.
          DEBUG(llvm::errs()
                << "AbstractDataFlowAcceleratorPass: Loop " << printLoop(Loop)
                << " is statically not dataflow because it "
                   "contains indirect call.\n");
          return false;
        }
        // Check if calling some supported math function.
        if (Callee->getName() != "sin") {
          // TODO: add a set for supported functions.
          DEBUG(llvm::errs()
                << "AbstractDataFlowAcceleratorPass: Loop " << printLoop(Loop)
                << " is statically not dataflow because it "
                   "contains unsupported call.\n");
          return false;
        }
      }
    }
  }

  // Done: this loop can be represented as data flow.
  // DEBUG(llvm::errs() << "isLoopDataFlow returned true.\n");
  return true;
}

DynamicDataFlow::DynamicDataFlow()
    : Loop(nullptr), PDF(nullptr), CurrentAge(0) {}

void DynamicDataFlow::configure(llvm::Loop *_Loop,
                                const PostDominanceFrontier *_PDF) {
  this->Loop = _Loop;
  this->PDF = _PDF;
  this->CurrentAge = 0;
}

void DynamicDataFlow::fixDependence(DynamicInstruction *DynamicInst,
                                    DataGraph *DG) {
  auto StaticInst = DynamicInst->getStaticInstruction();
  assert(StaticInst != nullptr &&
         "DynamicDataFlow cannot fix dependence for non llvm instructions.");

  // Only worried about control dependence.
  auto &CtrDeps = DG->CtrDeps.at(DynamicInst->Id);
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
}

void DynamicDataFlow::start() {
  this->StaticToDynamicMap.clear();
  this->CurrentAge = 0;
}

#undef DEBUG_TYPE

} // namespace

char AbstractDataFlowAcceleratorPass::ID = 0;
static llvm::RegisterPass<AbstractDataFlowAcceleratorPass>
    X("abs-data-flow-acc-pass", "Abstract datagraph accelerator transform pass",
      false, false);
