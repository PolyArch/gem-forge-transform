#ifndef LLVM_TDG_ABSTRACT_DATA_FLOW_ACCELERATOR_PASS_H
#define LLVM_TDG_ABSTRACT_DATA_FLOW_ACCELERATOR_PASS_H
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
#include "Utils.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include <iomanip>

/**
 * Represent normal instruction in the abstract dataflow graph.
 * This is actually a wrapper normal llvm instructions, but with more
 * information on the dependence type.
 *
 * Notice that this class takes the ownship of the original
 * LLVMDynamicInstuction.
 */
class AbsDataFlowLLVMInst : public DynamicInstruction {
public:
  AbsDataFlowLLVMInst(LLVMDynamicInstruction *_LLVMDynInst)
      : DynamicInstruction(_LLVMDynInst->getId()), LLVMDynInst(_LLVMDynInst) {}

  ~AbsDataFlowLLVMInst();

  std::string getOpName() const override {
    return this->LLVMDynInst->getOpName();
  }

  llvm::Instruction *getStaticInstruction() const override {
    return this->LLVMDynInst->getStaticInstruction();
  }

  LLVMDynamicInstruction *LLVMDynInst;

  void serializeToProtobufExtra(LLVM::TDG::TDGInstruction *ProtobufEntry,
                                DataGraph *DG) const override;

  /**
   * Extra dependence information.
   */
  std::unordered_set<DynamicId> UnrollableCtrDeps;
  std::unordered_set<DynamicId> PDFCtrDeps;
  std::unordered_set<DynamicId> IVDeps;
  std::unordered_set<DynamicId> RVDeps;
};

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
   * This must be an interator pointing to the datagraph.
   * We will wrap it with a AbsDataFlowLLVMInst and add more dependence
   * information to it.
   */
  void addDynamicInst(DataGraph::DynamicInstIter DynInstIter);

  void configure(llvm::Loop *_Loop, llvm::LoopInfo *LI,
                 const PostDominanceFrontier *_PDF, DataGraph *_DG,
                 TDGSerializer *_Serializer, CachedLoopUnroller *_CachedLU,
                 llvm::ScalarEvolution *_SE);
  void start();
  void end();

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

  const int UnrollWidth = 4;

  /**
   * Wrap the newly added dynamic llvm instruction with a AbsDataFlowLLVMInst,
   * and takes the place in the list.
   */
  AbsDataFlowLLVMInst *
  wrapWithAbsDataFlowLLVMInst(DataGraph::DynamicInstIter &DynInstIter) const;

  /**
   * Replace the control dependence with PDF control dependence.
   */
  void fixCtrDependence(AbsDataFlowLLVMInst *AbsDFInst);

  /**
   * Distinguish IV and RV dependence from general register dependence.
   */
  void fixRegDependence(AbsDataFlowLLVMInst *AbsDFInst);

  /**
   * Notice that this will update the CurrentAge.
   */
  void updateStaticToDynamicMap(DynamicInstruction *DynamicInst);
};

class AbstractDataFlowAcceleratorPass : public ReplayTrace {
public:
  static char ID;
  AbstractDataFlowAcceleratorPass() : ReplayTrace(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;

protected:
  bool initialize(llvm::Module &Module) override;

  bool finalize(llvm::Module &Module) override;

  void dumpStats(std::ostream &O) override;

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

  std::string DataFlowFileName;
  std::string RelativeDataFlowFileName;

  // Memorize the result of isDataflow.
  std::unordered_map<llvm::Loop *, bool> MemorizedLoopDataflow;

  struct DataFlowStats {
    std::string RegionId;
    uint64_t StaticInst;
    uint64_t DynamicInst;
    uint64_t DataFlowDynamicInst;
    uint64_t Config;
    uint64_t DataFlow;

    DataFlowStats(const std::string &_RegionId, uint64_t _StaticInst)
        : RegionId(_RegionId), StaticInst(_StaticInst), DynamicInst(0),
          DataFlowDynamicInst(0), Config(0), DataFlow(0) {}

    static void dumpStatsTitle(std::ostream &O) {
      O << std::setw(50) << "RegionId" << std::setw(20) << "StaticInsts"
        << std::setw(20) << "DynamicInst" << std::setw(20)
        << "DataFlowDynamicInst" << std::setw(20) << "Config" << std::setw(20)
        << "DataFlow" << '\n';
    }

    void dumpStats(std::ostream &O) const {
      O << std::setw(50) << this->RegionId << std::setw(20) << this->StaticInst
        << std::setw(20) << this->DynamicInst << std::setw(20)
        << this->DataFlowDynamicInst << std::setw(20) << this->Config
        << std::setw(20) << this->DataFlow << '\n';
    }
  };

  std::unordered_map<llvm::Loop *, DataFlowStats> Stats;

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

#endif