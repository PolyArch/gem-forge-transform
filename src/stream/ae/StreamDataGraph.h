#ifndef LLVM_TDG_ADDRESS_DATAGRAPH_H
#define LLVM_TDG_ADDRESS_DATAGRAPH_H

#include "ExecutionDataGraph.h"

#include "DataGraph.h"

class StaticStream;
class StreamDataGraph : public ExecutionDataGraph {
public:
  StreamDataGraph(const llvm::Loop *_Loop, const llvm::Value *_AddrValue,
                  std::function<bool(const llvm::PHINode *)> IsInductionVar);

  StreamDataGraph(const StreamDataGraph &Other) = delete;
  StreamDataGraph(StreamDataGraph &&Other) = delete;
  StreamDataGraph &operator=(const StreamDataGraph &Other) = delete;
  StreamDataGraph &operator=(StreamDataGraph &&Other) = delete;

  ~StreamDataGraph() {}

  /**
   * Key function to coalesce two memory streams.
   */
  bool isAbleToCoalesceWith(const StreamDataGraph &Other) const;

  void format(std::ostream &OStream) const;

  bool hasPHINodeInComputeInsts() const {
    return this->HasPHINodeInComputeInsts;
  }
  bool hasCallInstInComputeInsts() const {
    return this->HasCallInstInComputeInsts;
  }

  const llvm::Value *getAddrValue() const {
    return this->getSingleResultValue();
  }

  /**
   * Generate a function, but replacing the input with the map.
   * This is used to implement dependence on LoadComputeStream, which
   * may fuse some operations into itself.
   */
  using StreamSet = std::unordered_set<StaticStream *>;
  llvm::Function *
  generateFunctionWithFusedOp(const std::string &FuncName,
                              std::unique_ptr<llvm::Module> &Module,
                              const StreamSet &Streams, bool IsLoad = false);

  /**
   * Get the updated input after removing FusedOps from this datagraph.
   */
  ValueList getInputsWithFusedOp(const StreamSet &Streams);

  /**
   * Remove FusedOps from this datagraph.
   * @return A pair of updated inputs and compute instructions.
   */
  std::pair<ValueList, InstSet>
  sliceWithFusedOp(const StreamSet &Streams) const;

private:
  const llvm::Loop *Loop;
  std::unordered_set<const llvm::PHINode *> BaseIVs;
  std::unordered_set<const llvm::LoadInst *> BaseLoads;
  bool HasPHINodeInComputeInsts;
  bool HasCallInstInComputeInsts;

  void
  constructDataGraph(std::function<bool(const llvm::PHINode *)> IsInductionVar);
};

#endif