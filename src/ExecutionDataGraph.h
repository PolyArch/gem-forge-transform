#ifndef LLVM_TDG_EXECUTION_DATA_GRAPH_H
#define LLVM_TDG_EXECUTION_DATA_GRAPH_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"

#include <functional>
#include <list>
#include <ostream>
#include <unordered_map>
#include <unordered_set>

// Represent an executionn data graph.
class ExecutionDataGraph {
public:
  using InstSet = std::unordered_set<const llvm::Instruction *>;

  ExecutionDataGraph(const llvm::Value *_ResultValue)
      : ResultValue(_ResultValue) {}
  ExecutionDataGraph(const ExecutionDataGraph &Other) = delete;
  ExecutionDataGraph(ExecutionDataGraph &&Other) = delete;
  ExecutionDataGraph &operator=(const ExecutionDataGraph &Other) = delete;
  ExecutionDataGraph &operator=(ExecutionDataGraph &&Other) = delete;

  virtual ~ExecutionDataGraph() {}

  const llvm::Value *getResultValue() const { return this->ResultValue; }
  const std::list<const llvm::Value *> &getInputs() const {
    return this->Inputs;
  }
  const InstSet &getComputeInsts() const { return this->ComputeInsts; }
  bool hasCircle() const { return this->HasCircle; }

  /**
   * Generate a function takes the input and returns the value.
   * Returns the inserted function.
   */
  llvm::Function *
  generateComputeFunction(const std::string &FuncName,
                          std::unique_ptr<llvm::Module> &Module) const;

protected:
  const llvm::Value *ResultValue;
  std::list<const llvm::Value *> Inputs;
  std::unordered_set<const llvm::ConstantData *> ConstantDatas;
  InstSet ComputeInsts;
  bool HasCircle = false;

  using DFSTaskT = std::function<void(const llvm::Instruction *)>;
  void dfsOnComputeInsts(const llvm::Instruction *Inst, DFSTaskT Task) const;

  llvm::FunctionType *createFunctionType(llvm::Module *Module) const;

  using ValueMapT = std::unordered_map<const llvm::Value *, llvm::Value *>;
  void translate(llvm::IRBuilder<> &Builder, ValueMapT &ValueMap,
                 const llvm::Instruction *Inst) const;

  bool detectCircle() const;
};

#endif