#ifndef LLVM_TDG_DYNAMIC_TRACE_H
#define LLVM_TDG_DYNAMIC_TRACE_H

#include "TraceParser.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DynamicValue {
 public:
  DynamicValue(const std::string& _Value, const std::string& _MemBase = "",
               uint64_t _MemOffset = 0);
  DynamicValue(const DynamicValue& Other);
  std::string Value;
  // Base/Offset of memory address.
  std::string MemBase;
  uint64_t MemOffset;
};

class DynamicInstruction {
 public:
  using DynamicId = uint64_t;

  DynamicInstruction();
  // Not copiable.
  DynamicInstruction(const DynamicInstruction& other) = delete;
  DynamicInstruction& operator=(const DynamicInstruction& other) = delete;
  // Not Movable.
  DynamicInstruction(DynamicInstruction&& other) = delete;
  DynamicInstruction& operator=(DynamicInstruction&& other) = delete;

  virtual ~DynamicInstruction();
  virtual const std::string& getOpName() = 0;
  virtual llvm::Instruction* getStaticInstruction() { return nullptr; }
  virtual void dump() {}

  const DynamicId Id;

  DynamicValue* DynamicResult;

  DynamicInstruction* Prev;
  DynamicInstruction* Next;

  // This is important to store some constant/non-instruction generated
  // operands
  std::vector<DynamicValue*> DynamicOperands;

 private:
  static DynamicId allocateId();
};

class LLVMDynamicInstruction : public DynamicInstruction {
 public:
  LLVMDynamicInstruction(llvm::Instruction* _StaticInstruction,
                         TraceParser::TracedInst& _Parsed,
                         DynamicInstruction* _Prev, DynamicInstruction* _Next);
  LLVMDynamicInstruction(llvm::Instruction* _StaticInstruction,
                         DynamicValue* _Result,
                         std::vector<DynamicValue*> _Operands,
                         DynamicInstruction* _Prev, DynamicInstruction* _Next);
  // Not copiable.
  LLVMDynamicInstruction(const LLVMDynamicInstruction& other) = delete;
  LLVMDynamicInstruction& operator=(const LLVMDynamicInstruction& other) =
      delete;
  // Not Movable.
  LLVMDynamicInstruction(LLVMDynamicInstruction&& other) = delete;
  LLVMDynamicInstruction& operator=(LLVMDynamicInstruction&& other) = delete;
  const std::string& getOpName() override { return this->OpName; }
  llvm::Instruction* getStaticInstruction() override {
    return this->StaticInstruction;
  }
  llvm::Instruction* StaticInstruction;
  std::string OpName;
};

class DataGraph {
 public:
  using DynamicId = DynamicInstruction::DynamicId;

  // The datagraph owns the parser, but certainly not the module.
  DataGraph(llvm::Module* _Module);
  ~DataGraph();

  DataGraph(const DataGraph& other) = delete;
  DataGraph& operator=(const DataGraph& other) = delete;
  DataGraph(DataGraph&& other) = delete;
  DataGraph& operator=(DataGraph&& other) = delete;

  DynamicInstruction* DynamicInstructionListHead;
  DynamicInstruction* DynamicInstructionListTail;

  // A map from dynamic id to the actual insts.
  std::unordered_map<DynamicId, DynamicInstruction*> AliveDynamicInstsMap;

  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> RegDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> CtrDeps;
  std::unordered_map<DynamicId, std::unordered_set<DynamicId>> MemDeps;

  /**
   * Map from llvm static instructions to the last dynamic instruction id.
   * If missing, then it hasn't appeared in the dynamic trace.
   * Only for non-phi node.
   * Used for register dependence resolution.
   **/
  std::unordered_map<llvm::Instruction*, DynamicId> StaticToLastDynamicMap;

  llvm::Module* Module;

  llvm::DataLayout* DataLayout;

  bool IsSimpleMode;

  // Some statistics.
  uint64_t NumMemDependences;

  DynamicInstruction* getDynamicInstFromId(DynamicId Id) const;

  class DynamicFrame {
   public:
    llvm::Function* Function;

    // A tiny run time environment, basically for memory base/offset
    // computation.
    std::unordered_map<llvm::Value*, DynamicValue> RunTimeEnv;
    llvm::BasicBlock* PrevBasicBlock;
    llvm::CallInst* PrevCallInst;
    explicit DynamicFrame(
        llvm::Function* _Function,
        std::unordered_map<llvm::Value*, DynamicValue>&& _Arguments);
    ~DynamicFrame();

    const DynamicValue& getValue(llvm::Value* Value) const;

    DynamicFrame(const DynamicFrame& other) = delete;
    DynamicFrame(DynamicFrame&& other) = delete;
    DynamicFrame& operator=(const DynamicFrame& other) = delete;
    DynamicFrame& operator=(DynamicFrame&& other) = delete;
  };

  // Load one more inst from the trace. nullptr if eof.
  DynamicInstruction* loadOneDynamicInst();
  // Commit one inst and remove it from the alive set.
  void commitOneDynamicInst();

 private:
  /**********************************************************************/
  /* These are temporary fields used in construnction only.
  /**********************************************************************/
  // Parse the dynamic instruction in the line buffer.
  void parseDynamicInstruction(TraceParser::TracedInst& Parsed);
  void parseFunctionEnter(TraceParser::TracedFuncEnter& Parsed);

  // Records the information of the dynamic value inter-frame.
  // Especially used for mem/base initialization.
  std::list<DynamicFrame> DynamicFrameStack;

  TraceParser* Parser;

  // To resovle phi node.
  std::unordered_map<llvm::PHINode*, DynamicId> PhiNodeDependenceMap;

  /**
   * Handle phi node and effectively remove it from the graph.
   * 1. Determine the incoming block and incoming value, using PrevBasicBlock.
   * 2. Determine the incoming non-phi instruction, if any.
   * 3. Initialize the dynamic value, handle the memory base/offset for it.
   */
  void handlePhiNode(llvm::PHINode* StaticPhi,
                     const TraceParser::TracedInst& Parsed);

  std::string CurrentBasicBlockName;
  int CurrentIndex;

  llvm::BasicBlock* CurrentBasicBlock;
  llvm::BasicBlock::iterator CurrentStaticInstruction;

  llvm::Instruction* getLLVMInstruction(const std::string& FunctionName,
                                        const std::string& BasicBlockName,
                                        const int Index);

  DynamicInstruction* getPreviousBranchDynamicInstruction(
      DynamicInstruction* DynamicInst);

  // A map from virtual address to the last dynamic store instructions that
  // writes to it.
  std::unordered_map<uint64_t, DynamicId> AddrToLastStoreInstMap;
  // A map from virtual address to the last dynamic load instructions that
  // reads from it.
  std::unordered_map<uint64_t, DynamicId> AddrToLastLoadInstMap;

  // Handle the RAW, WAW, WAR dependence.
  void handleMemoryDependence(DynamicInstruction* DynamicInst);

  void handleControlDependence(DynamicInstruction* DynamicInst);

  // Return true if there is a dependence.
  bool checkAndAddMemoryDependence(
      std::unordered_map<uint64_t, DynamicId>& LastMap, uint64_t Addr,
      DynamicInstruction* DynamicInst);

  void handleRegisterDependence(DynamicInstruction* DynamicInst,
                                llvm::Instruction* StaticInstruction);

  // Handle the memory address base/offset.
  // Very important to be replayable in gem5.
  void handleMemoryBase(DynamicInstruction* DynamicInst);

  /**
   * Get a unique name for this instruction.
   * This is typically used for instructions which will create new memory base.
   * In the replay phase, the simulator does not maintain a frame stack, but
   * only a unified map for all the memory bases. In order to make sure that the
   * base is unique to this static instruction and not confused with other
   * instructions with same name but from different functions, we append the
   * function name before it.
   */
  std::unordered_map<llvm::Instruction*, std::string>
      StaticInstructionUniqueNameMap;
  const std::string& getUniqueNameForStaticInstruction(
      llvm::Instruction* StaticInst);
};

#endif