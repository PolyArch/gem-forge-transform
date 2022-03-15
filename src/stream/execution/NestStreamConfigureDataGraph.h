#ifndef GEM_FORGE_NEST_STREAM_CONFIGURE_DATAGRAPH_H
#define GEM_FORGE_NEST_STREAM_CONFIGURE_DATAGRAPH_H

#include "ExecutionDataGraph.h"

/**
 * Ananlyze the configuration block.
 */

class NestStreamConfigureDataGraph : public ExecutionDataGraph {
public:
  NestStreamConfigureDataGraph(const llvm::Loop *_OuterLoop,
                               const llvm::BasicBlock *_ConfigBB);

  ~NestStreamConfigureDataGraph() {}

  const std::string &getFuncName() const { return this->FuncName; }
  bool isValid() const { return this->IsValid; }

private:
  const llvm::Loop *OuterLoop;
  const llvm::BasicBlock *ConfigBB;
  const std::string FuncName;
  bool IsValid = false;

  void constructDataGraph();

  /**
   * To reduce the number of inputs, we apply a simple peephole optimization
   * to remove an out-of-loop instruction as input if all of its operands are
   * either constant or already an inputs. We mark this instruction as a
   * ComputeInst.
   *
   * We iterate until we found no such input instructions.
   */
  void
  hoistInputToCompute(std::unordered_set<const llvm::Value *> &UnsortedInputs);
};

#endif