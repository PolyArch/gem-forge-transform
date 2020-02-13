#ifndef LLVM_TDG_ADDRESS_DATAGRAPH_H
#define LLVM_TDG_ADDRESS_DATAGRAPH_H

#include "ExecutionDataGraph.h"

#include "DataGraph.h"

class AddressDataGraph : public ExecutionDataGraph {
public:
  AddressDataGraph(const llvm::Loop *_Loop, const llvm::Value *_AddrValue,
                   std::function<bool(const llvm::PHINode *)> IsInductionVar);

  AddressDataGraph(const AddressDataGraph &Other) = delete;
  AddressDataGraph(AddressDataGraph &&Other) = delete;
  AddressDataGraph &operator=(const AddressDataGraph &Other) = delete;
  AddressDataGraph &operator=(AddressDataGraph &&Other) = delete;

  ~AddressDataGraph() {}

  /**
   * Key function to coalesce two memory streams.
   */
  bool isAbleToCoalesceWith(const AddressDataGraph &Other) const;

  void format(std::ostream &OStream) const;

  bool hasPHINodeInComputeInsts() const {
    return this->HasPHINodeInComputeInsts;
  }
  bool hasCallInstInComputeInsts() const {
    return this->HasCallInstInComputeInsts;
  }

  const llvm::Value *getAddrValue() const { return this->ResultValue; }

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