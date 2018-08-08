#ifndef LLVM_TDG_MEMORY_ACCESS_PATTERN_H
#define LLVM_TDG_MEMORY_ACCESS_PATTERN_H

#include "llvm/IR/Instruction.h"

#include <unordered_map>

/**
 * This class try to compute the dynamic memory access pattern within a loop.
 */
class MemoryAccessPattern {
public:
  MemoryAccessPattern() = default;
  MemoryAccessPattern(const MemoryAccessPattern &Other) = delete;
  MemoryAccessPattern(MemoryAccessPattern &&Other) = delete;
  MemoryAccessPattern &operator=(const MemoryAccessPattern &Other) = delete;
  MemoryAccessPattern &operator=(MemoryAccessPattern &&Other) = delete;

  /**
   * Add one dynamic access address for a specific instruction.
   */
  void addAccess(llvm::Instruction *Inst, uint64_t Addr);

  /**
   * Wraps up an ongoing stream.
   */
  void endLoop(llvm::Instruction *Inst, size_t Iters);

  enum Pattern {
    RANDOM,
    INDIRECT,
    CONSTANT,
    LINEAR,
    QUARDRIC,
  };

  struct PatternComputation {
    Pattern CurrentPattern;
    // Statistics.
    uint64_t Count;
    uint64_t StreamCount;
    // INDIRECT
    llvm::Instruction *BaseLoad;
    // CONSTANT.
    uint64_t Base;
    // LINEAR.
    int64_t StrideI;
    uint64_t I;
    // QUARDRIC.
    uint64_t NI;
    int64_t StrideJ;
    uint64_t J;

    PatternComputation(uint64_t _Base)
        : CurrentPattern(CONSTANT), Count(1), StreamCount(0), BaseLoad(nullptr),
          Base(_Base), StrideI(0), I(0), NI(0), StrideJ(0), J(0) {}

    PatternComputation(const PatternComputation &Other)
        : CurrentPattern(Other.CurrentPattern), Count(Other.Count),
          StreamCount(Other.StreamCount), BaseLoad(Other.BaseLoad),
          Base(Other.Base), StrideI(Other.StrideI), I(Other.I), NI(Other.NI),
          StrideJ(Other.StrideJ), J(Other.J) {}
  };

  const PatternComputation &getPattern(llvm::Instruction *Inst) const;

  bool contains(llvm::Instruction *Inst) const;

  static std::string formatPattern(Pattern Pat);

private:
  std::unordered_map<llvm::Instruction *, PatternComputation>
      ComputedPatternMap;

  /**
   * Representing an ongoing computation.
   */

  std::unordered_map<llvm::Instruction *, PatternComputation>
      ComputingPatternMap;

  llvm::Instruction *isIndirect(llvm::Instruction *Inst) const;
};

#endif