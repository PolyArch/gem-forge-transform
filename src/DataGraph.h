#ifndef LLVM_TDG_DYNAMIC_TRACE_H
#define LLVM_TDG_DYNAMIC_TRACE_H

#include "DynamicInstruction.h"
#include "trace/TraceParser.h"

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern llvm::cl::opt<std::string> TraceFileName;
extern llvm::cl::opt<std::string> TraceFileFormat;
extern llvm::cl::opt<std::string> InstUIDFileName;

/**
 * AddrToMemAccessMap maintains a map from memory address to the last access
 * instruction's id.
 * This is used to calculate the memory dependence.
 * It maintains an update log and when the log's size grows above a threshold,
 * it will erase the oldest update to release space.
 * This assumes that the user will not care about long range memory dependence,
 * e.g. 1000000 instruction distance.
 */
class AddrToMemAccessMap {
public:
  AddrToMemAccessMap() = default;
  AddrToMemAccessMap(const AddrToMemAccessMap &Other) = delete;
  AddrToMemAccessMap(AddrToMemAccessMap &&Other) = delete;
  AddrToMemAccessMap &operator=(const AddrToMemAccessMap &Other) = delete;
  AddrToMemAccessMap &operator=(AddrToMemAccessMap &&Other) = delete;

  using DynamicId = DynamicInstruction::DynamicId;
  using Address = uint64_t;

  void update(Address Addr, DynamicId Id);

  /**
   * Get the last instruction's id which accessed Addr.
   * Return InvalidId if there has not been any.
   */
  DynamicId getLastAccess(Address Addr) const;

  size_t size() const { return this->Map.size(); }

private:
  using LogEntry = std::pair<Address, DynamicId>;
  std::list<LogEntry> Log;
  std::unordered_map<Address, DynamicId> Map;
  static const size_t LOG_THRESHOLD = 10000000;

  void release();
};

/**
 * Compute the memory dependence.
 */
class MemoryDependenceComputer {
public:
  MemoryDependenceComputer() = default;
  MemoryDependenceComputer(const MemoryDependenceComputer &Other) = delete;
  MemoryDependenceComputer(MemoryDependenceComputer &&Other) = delete;
  MemoryDependenceComputer &
  operator=(const MemoryDependenceComputer &Other) = delete;
  MemoryDependenceComputer &
  operator=(MemoryDependenceComputer &&Other) = delete;

  using Address = AddrToMemAccessMap::Address;
  using DynamicId = DynamicInstruction::DynamicId;

  void update(Address Addr, size_t ByteSize, DynamicId Id, bool IsLoad);
  void getMemoryDependence(Address Addr, size_t ByteSize, bool IsLoad,
                           std::unordered_set<DynamicId> &OutMemDeps) const;

  size_t loadMapSize() const { return LoadMap.size(); }
  size_t storeMapSize() const { return StoreMap.size(); }

private:
  AddrToMemAccessMap LoadMap;
  AddrToMemAccessMap StoreMap;
};

class DataGraph {
public:
  using DynamicId = DynamicInstruction::DynamicId;

  enum DataGraphDetailLv {
    SIMPLE,
    STANDALONE,
    INTEGRATED,
  };

  // The datagraph owns the parser, but certainly not the module.
  // You can also control if its detail level.
  DataGraph(llvm::Module *_Module, DataGraphDetailLv _DetailLevel);
  ~DataGraph();

  DataGraph(const DataGraph &other) = delete;
  DataGraph &operator=(const DataGraph &other) = delete;
  DataGraph(DataGraph &&other) = delete;
  DataGraph &operator=(DataGraph &&other) = delete;

  std::list<DynamicInstruction *> DynamicInstructionList;
  using DynamicInstIter = std::list<DynamicInstruction *>::iterator;

  /**
   * Returns a pointer to the alive dynamic instruction.
   * nullptr if not found.
   */
  DynamicInstruction *getAliveDynamicInst(DynamicId Id);
  // A map from dynamic id to the actual insts.
  std::unordered_map<DynamicId, DynamicInstIter> AliveDynamicInstsMap;

  /**
   * These maps hold the dependence for *ALIVE* instructions.
   * We guarantee that there is always an empty set to represent no dependence.
   * Make sure you maintain this property.
   *
   * For the register dependence, we use list<pair<>> to store the corresponding
   * operand. This is handy for many transformations, e.g. loop unroll to break
   * dependence on induction variable.
   * This is not a set, e.g. for SIMD instruction is is possible that one
   * operand has multiple dependence. For those non-llvm-inst dependence, the
   * static instruction pointer can be nullptr.
   */
  using DependenceSet = std::unordered_set<DynamicId>;
  using DependenceMap = std::unordered_map<DynamicId, DependenceSet>;
  using RegDependence = std::pair<llvm::Instruction *, DynamicId>;
  using RegDependenceList = std::list<RegDependence>;
  std::unordered_map<DynamicId, RegDependenceList> RegDeps;
  DependenceMap CtrDeps;
  DependenceMap MemDeps;

  llvm::Module *Module;

  llvm::DataLayout *DataLayout;

  DataGraphDetailLv DetailLevel;

  // Some statistics.
  uint64_t NumMemDependences;

  DynamicInstIter getDynamicInstFromId(DynamicId Id) const;

  class DynamicFrame {
  public:
    llvm::Function *Function;

    // A tiny run time environment, basically for memory base/offset
    // computation.
    std::unordered_map<const llvm::Value *, DynamicValue> RunTimeEnv;

    // Stores the dynamic value of CallInst or InvokeInst.
    // Could be consumed by parseFuncEnter.
    std::vector<DynamicValue> CallStack;
    llvm::Instruction *PrevCallInst;
    explicit DynamicFrame(
        llvm::Function *_Function,
        std::unordered_map<const llvm::Value *, DynamicValue> &&_Arguments);
    ~DynamicFrame();

    const DynamicValue &getValue(const llvm::Value *Value) const;
    const DynamicValue *getValueNullable(const llvm::Value *Value) const;
    void insertValue(const llvm::Value *Value, DynamicValue DValue);

    DynamicFrame(const DynamicFrame &other) = delete;
    DynamicFrame(DynamicFrame &&other) = delete;
    DynamicFrame &operator=(const DynamicFrame &other) = delete;
    DynamicFrame &operator=(DynamicFrame &&other) = delete;

    /********************************************************************
     * Accessors.
     ********************************************************************/

    // Previous BB is used for resolve phi node.
    llvm::BasicBlock *getPrevBB() const { return this->PrevBasicBlock; }
    void setPrevBB(llvm::BasicBlock *PrevBB) {
      assert(PrevBB->getParent() == this->Function &&
             "This BB is not in function.");
      this->PrevBasicBlock = PrevBB;
    }

    const std::list<DynamicId> &
    translateRegisterDependence(llvm::Instruction *StaticInst) const;
    void updateRegisterDependenceLookUpMap(llvm::Instruction *StaticInst,
                                           std::list<DynamicId> Ids);
    void updateRegisterDependenceLookUpMap(llvm::Instruction *StaticInst,
                                           DynamicId Id);
    size_t getRegisterDependenceLookUpMapSize() const {
      return this->RegDepLookUpMap.size();
    }

  private:
    llvm::BasicBlock *PrevBasicBlock;

    /**
     * Map from llvm static instructions I to a list of dynamic ids Ids.
     * Any register dependence to I will be translated to dependences on Ids.
     *
     * For flexibility, we allow multiple ids here. One possible use case is
     * when we want to delete one dynamic instruction but also maintain the
     * corrent dependence chain.
     *
     * Special case for PHINode: Since we don't include PHINode in our graph,
     * there is no dynamic id allocated for PHINode. Instead, if there is no
     * other user defined transformation, here we store the dynamic id of the
     * producer of the incoming value.
     */
    std::unordered_map<llvm::Instruction *, std::list<DynamicId>>
        RegDepLookUpMap;
  };

  // Records the information of the dynamic value inter-frame.
  // Grows from the front side.
  // Especially used for mem/base initialization.
  std::list<DynamicFrame> DynamicFrameStack;

  std::unordered_map<const llvm::Value *, DynamicValue> GlobalValueEnv;

  // Load one more inst from the trace.
  // Return DynamicInstructionList.end() if eof.
  DynamicInstIter loadOneDynamicInst();
  // Commit one inst and remove it from the alive set.
  void commitOneDynamicInst();
  void commitDynamicInst(DynamicId Id);
  // Insert one instruction into the list and initialize the dependence and
  // alive map.
  DynamicInstIter insertDynamicInst(DynamicInstIter InsertBefore,
                                    DynamicInstruction *DynamicInst);

  void updateAddrToLastMemoryAccessMap(uint64_t Addr, size_t ByteSize,
                                       DynamicId Id, bool IsLoad);

  void updatePrevControlInstId(DynamicInstruction *DynamicInst);

private:
  /**********************************************************************
   * These are temporary fields used in construnction only.
   */

  /**********************************************************************
   *
   * Parse the dynamic instruction in the line buffer.
   * Return true if we get an non-phi instruction, false otherwise.
   * As phi instruction will not be added to the graph.
   */
  bool parseDynamicInstruction(TraceParser::TracedInst &Parsed);
  void parseFunctionEnter(TraceParser::TracedFuncEnter &Parsed);

  TraceParser *Parser;

  /**
   * Handle phi node and effectively remove it from the graph.
   * 1. Determine the incoming block and incoming value, using PrevBasicBlock.
   * 2. Determine the incoming non-phi instruction, if any.
   * 3. Initialize the dynamic value, handle the memory base/offset for it.
   */
  void handlePhiNode(llvm::PHINode *StaticPhi,
                     const TraceParser::TracedInst &Parsed);

  /***************************************************************************
   * Find the static llvm instruction for an incoming dynamic instruction.
   ***************************************************************************/

  // Memorized map to quickly locate the static instruction.
  std::unordered_map<
      llvm::Function *,
      std::unordered_map<std::string, std::vector<llvm::Instruction *>>>
      MemorizedStaticInstMap;

  // Use MemorizedStaticInstMap to quickly locate the static instruction.
  // May update the memorize map if this is the first time.
  // Also if the frame stack is empty, trys to allocate an empty frame for it.
  llvm::Instruction *getLLVMInstruction(const std::string &FunctionName,
                                        const std::string &BasicBlockName,
                                        const int Index);

  /***************************************************************************
   * Handle memory dependence.
   ***************************************************************************/

  // Helper class to compute the memory dependence.
  MemoryDependenceComputer MemDepsComputer;
  // Handle the RAW, WAW, WAR dependence.
  void handleMemoryDependence(DynamicInstruction *DynamicInst);

  /***************************************************************************
   * Handle control dependence.
   ***************************************************************************/

  // Store the previous control flow instruction id.
  // Used to resolve control dependence.
  DynamicId PrevControlInstId;
  void handleControlDependence(DynamicInstruction *DynamicInst);

  void handleRegisterDependence(DynamicInstruction *DynamicInst,
                                llvm::Instruction *StaticInstruction);

  // Handle the memory address base/offset.
  // Very important to be replayable in gem5.
  void handleMemoryBase(DynamicInstruction *DynamicInst);

  /**
   * Get a unique name for this instruction.
   * This is typically used for instructions which will create new memory base.
   * In the replay phase, the simulator does not maintain a frame stack, but
   * only a unified map for all the memory bases. In order to make sure that the
   * base is unique to this static instruction and not confused with other
   * instructions with same name but from different functions, we append the
   * function name before it.
   */
  std::unordered_map<llvm::Instruction *, std::string>
      StaticInstructionUniqueNameMap;
  const std::string &
  getUniqueNameForStaticInstruction(llvm::Instruction *StaticInst);

  /**
   * Utility function to print a static instructions.
   */
  void printStaticInst(llvm::raw_ostream &O,
                       llvm::Instruction *StaticInst) const;

  /**
   * Print the memory usage so far.
   * This is used to debug memory leak...
   */
  void printMemoryUsage() const;
};

#endif