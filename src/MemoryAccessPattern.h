#ifndef LLVM_TDG_MEMORY_ACCESS_PATTERN_H
#define LLVM_TDG_MEMORY_ACCESS_PATTERN_H

#include "llvm/IR/Instruction.h"

#include <unordered_map>

/**
 * This class try to compute the dynamic memory access pattern within a loop.
 *
 * Pattern hierarchy.
 * UNKNOWN -> CONSTANT -> LINEAR -> QUARDRIC -> RANDOM
 *         -> INDIRECT
 *
 * A pattern starts from UNKNOWN, representing that there is no memory access
 * seen so far. As the user provides more accessed address, we gradually relax
 * our constraints to RANDOM.
 *
 * An indirect stream is identified by checking if its address comes from a
 * load.
 *
 * When one stream ends, the user calls endStream() to update a stream's
 * pattern. If there is some previously computed pattern, it can only be further
 * relaxed.
 *
 * Also, the user can also call addMissingAccess to inform that there is a
 * missing access in this iteration. This will mark the stream conditional.
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
   * There is one missing access in this iteration.
   * We implicitly assume the missing access complys with the current pattern.
   */
  void addMissingAccess(llvm::Instruction *Inst);

  /**
   * Wraps up an ongoing stream.
   */
  void endStream(llvm::Instruction *Inst);

  /**
   * Finalize the pattern computed.
   * This will compute the average length of the stream and if
   * that is lower than a threshold, we think this is random pattern?
   */
  void finializePattern();

  /**
   */
  enum Pattern {
    UNKNOWN,
    INDIRECT,
    CONSTANT,
    LINEAR,
    QUARDRIC,
    RANDOM,
  };

  struct PatternComputation {
    Pattern CurrentPattern;
    // Statistics.
    uint64_t Count;
    uint64_t Iters;
    bool IsConditional;
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

    /**
     * Constructor for missing first access.
     */
    PatternComputation()
        : CurrentPattern(UNKNOWN), Count(1), Iters(1), IsConditional(true),
          BaseLoad(nullptr), Base(0), StrideI(0), I(0), NI(0), StrideJ(0),
          J(0) {}

    /**
     * Constructor for known first access.
     */
    PatternComputation(uint64_t _Base)
        : CurrentPattern(CONSTANT), Count(1), Iters(1), IsConditional(false),
          BaseLoad(nullptr), Base(_Base), StrideI(0), I(0), NI(0), StrideJ(0),
          J(0) {}
  };

  struct ComputedPattern {
    Pattern CurrentPattern;
    bool IsConditional;
    uint64_t Count;
    uint64_t Iters;
    uint64_t StreamCount;
    // For indirect memory access.
    llvm::Instruction *BaseLoad;
    ComputedPattern(const PatternComputation &NewPattern)
        : CurrentPattern(NewPattern.CurrentPattern),
          IsConditional(NewPattern.IsConditional), Count(NewPattern.Count),
          Iters(NewPattern.Iters), StreamCount(1),
          BaseLoad(NewPattern.BaseLoad) {}

    void merge(const PatternComputation &NewPattern);
  };

  const ComputedPattern &getPattern(llvm::Instruction *Inst) const;

  bool contains(llvm::Instruction *Inst) const;

  static std::string formatPattern(Pattern Pat);

private:
  std::unordered_map<llvm::Instruction *, ComputedPattern> ComputedPatternMap;

  /**
   * Representing an ongoing computation.
   */

  std::unordered_map<llvm::Instruction *, PatternComputation>
      ComputingPatternMap;

  llvm::Instruction *isIndirect(llvm::Instruction *Inst) const;

  /**
   * Implement the pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A)
   */
  static bool isPatternRelaxed(Pattern A, Pattern B);
};

#endif