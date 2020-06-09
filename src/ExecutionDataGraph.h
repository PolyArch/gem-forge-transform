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
   * A hack to extend the result value with a tail Atomic instruction.
   * Can be AtomicRMW or AtomicCmpXchg. Then the DataGraph will generate two
   * functions, one for loaded value and one for stored value.
   * e.g. for atomiccmpxchg stream, the __store() function returns the final
   * value stored to the place, and __load() function returns the value for
   * the core.
   *
   * The Atomic instruction is transformed to its normal version, with
   * the input as the last function argument and result as returned value.
   */
  void extendTailAtomicInst(
      const llvm::Instruction *AtomicInst,
      const std::vector<const llvm::Instruction *> &FusedLoadOps);
  const llvm::Instruction *getTailAtomicInst() const {
    return this->TailAtomicInst;
  }
  bool hasTailAtomicInst() const { return this->TailAtomicInst != nullptr; }

  llvm::Type *getReturnType(bool IsLoad) const;

  /**
   * Generate a function takes the input and returns the value.
   * @param IsLoad: Only useful when there is a tail atomic.
   * Returns the inserted function.
   */
  llvm::Function *generateFunction(const std::string &FuncName,
                                   std::unique_ptr<llvm::Module> &Module,
                                   bool IsLoad = false) const;

protected:
  const llvm::Value *ResultValue;
  std::list<const llvm::Value *> Inputs;
  std::unordered_set<const llvm::ConstantData *> ConstantDatas;
  InstSet ComputeInsts;
  const llvm::Instruction *TailAtomicInst = nullptr;
  std::vector<const llvm::Instruction *> FusedLoadOps;
  bool HasCircle = false;

  using DFSTaskT = std::function<void(const llvm::Instruction *)>;
  void dfsOnComputeInsts(const llvm::Instruction *Inst, DFSTaskT Task) const;

  llvm::FunctionType *createFunctionType(llvm::Module *Module,
                                         bool IsLoad) const;

  using ValueMapT = std::unordered_map<const llvm::Value *, llvm::Value *>;
  void translate(llvm::IRBuilder<> &Builder, ValueMapT &ValueMap,
                 const llvm::Instruction *Inst) const;

  bool detectCircle() const;

  llvm::Value *generateTailAtomicRMW(llvm::IRBuilder<> &Builder,
                                     llvm::Value *AtomicArg,
                                     llvm::Value *RhsArg, bool IsLoad) const;
  llvm::Value *generateTailAtomicCmpXchg(llvm::IRBuilder<> &Builder,
                                         llvm::Value *AtomicArg,
                                         llvm::Value *CmpValue,
                                         llvm::Value *XchgValue,
                                         bool IsLoad) const;
};

#endif