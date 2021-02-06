#ifndef LLVM_TDG_ADDRESS_DATAGRAPH_H
#define LLVM_TDG_ADDRESS_DATAGRAPH_H

#include "ExecutionDataGraph.h"

#include "DataGraph.h"

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