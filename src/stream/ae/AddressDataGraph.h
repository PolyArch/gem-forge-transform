#ifndef LLVM_TDG_ADDRESS_DATAGRAPH_H
#define LLVM_TDG_ADDRESS_DATAGRAPH_H

#include "DataGraph.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"

#include <functional>
#include <list>
#include <ostream>
#include <unordered_map>
#include <unordered_set>

class AddressDataGraph {
public:
  AddressDataGraph(const llvm::Loop *_Loop, const llvm::Value *_AddrValue,
                   std::function<bool(const llvm::PHINode *)> IsInductionVar);

  void format(std::ostream &OStream) const;

  bool hasCircle() const { return this->HasCircle; }

  const std::list<const llvm::Value *> &getInputs() const {
    return this->Inputs;
  }

  const llvm::Value *getAddrValue() const { return this->AddrValue; }

  /**
   * Generate a function takes the input and returns the value.
   * Returns the inserted function.
   */
  llvm::Function *
  generateComputeFunction(const std::string &FuncName,
                          std::unique_ptr<llvm::Module> &Module) const;

private:
  const llvm::Loop *Loop;
  const llvm::Value *AddrValue;
  std::list<const llvm::Value *> Inputs;
  std::unordered_set<const llvm::PHINode *> BaseIVs;
  std::unordered_set<const llvm::LoadInst *> BaseLoads;
  std::unordered_set<const llvm::ConstantData *> ConstantDatas;
  std::unordered_set<const llvm::Instruction *> ComputeInsts;
  bool HasCircle;

  void
  constructDataGraph(std::function<bool(const llvm::PHINode *)> IsInductionVar);

  using DFSTaskT = std::function<void(const llvm::Instruction *)>;
  void dfsOnComputeInsts(const llvm::Instruction *Inst, DFSTaskT Task) const;

  bool detectCircle() const;

  llvm::FunctionType *createFunctionType(llvm::Module *Module) const;

  void
  translate(llvm::IRBuilder<> &Builder,
            std::unordered_map<const llvm::Value *, llvm::Value *> &ValueMap,
            const llvm::Instruction *Inst) const;
};

#endif