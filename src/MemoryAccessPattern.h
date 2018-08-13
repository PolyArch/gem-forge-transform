#ifndef LLVM_TDG_MEMORY_ACCESS_PATTERN_H
#define LLVM_TDG_MEMORY_ACCESS_PATTERN_H

#include "llvm/IR/Instruction.h"

#include <unordered_map>

/**
 * This class try to compute the dynamic memory stream within a loop.
 *
 * A stream is conceptually divided into two parts:
 * 1. The address pattern describes the address of the memory access. It ignores
 * all the information of control flow. The address pattern has a simple
 * hierarchy.
 *
 * UNKNOWN -> CONSTANT -> LINEAR -> QUARDRIC -> RANDOM
 *         -> INDIRECT
 *
 * The address pattern starts from UNKNOWN, representing that there is no memory
 * access seen so far. As the user provides more accessed address, we gradually
 * relax our constraints to RANDOM.
 *
 * An indirect address pattern is identified by checking if its address comes
 * from a load.
 *
 * 2. The access pattern describes how the address is accessed. It has a five
 * categories:
 *
 *               -> ConditionalAccessOnly
 * Unconditional                          -> SameCondition -> Independent
 *               -> ConditionalUpdateOnly
 *
 * An unconditional access pattern can be represented as pseudo-code:
 * while (i < end) {
 *   a[i];
 *   i++;
 * }
 *
 * And other access patterns can be understood by putting if condition on access
 * (a[i]) and/or update (i++).
 *
 * NOTE: however, since so far we only have the access information, not updating
 * information, we can not support independent access patter.
 *
 * The user will feed in memory addesses via addAccess(), or addMissingAccess if
 * missing access in that iteration. For each computing stream, we maintain one
 * FSM for each address pattern and access pattern.
 *
 * When one stream ends, the user calls endStream() and we check these FSMs to
 * find the succeeded patterns. If there is some previously computed pattern, it
 * can only be further relaxed in the hierarchy.
 *
 */
class MemoryAccessPattern {
public:
  MemoryAccessPattern(llvm::Instruction *_MemInst)
      : MemInst(_MemInst), ComputedPatternPtr(nullptr) {}
  ~MemoryAccessPattern() {
    if (this->ComputedPatternPtr != nullptr) {
      delete this->ComputedPatternPtr;
      this->ComputedPatternPtr = nullptr;
    }
  }
  MemoryAccessPattern(const MemoryAccessPattern &Other) = delete;
  MemoryAccessPattern(MemoryAccessPattern &&Other) = delete;
  MemoryAccessPattern &operator=(const MemoryAccessPattern &Other) = delete;
  MemoryAccessPattern &operator=(MemoryAccessPattern &&Other) = delete;

  /**
   * Add one dynamic access address for a specific instruction.
   */
  void addAccess(uint64_t Addr);

  /**
   * There is one missing access in this iteration.
   * We implicitly assume the missing access complys with the current pattern.
   */
  void addMissingAccess();

  /**
   * Wraps up an ongoing stream.
   */
  void endStream();

  /**
   * Finalize the pattern computed.
   * This will compute the average length of the stream and if
   * that is lower than a threshold, we think this is random pattern?
   */
  void finalizePattern();

  bool computed() const;

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

  enum AccessPattern {
    UNCONDITIONAL,
    CONDITIONAL_ACCESS_ONLY,
    CONDITIONAL_UPDATE_ONLY,
    SAME_CONDITION,
    // Independent access pattern is so far not supported.
  };

  static std::string formatPattern(Pattern Pat);
  static std::string formatAccessPattern(AccessPattern AccPattern);

private:
  class AddressPatternFSM {
  public:
    Pattern CurrentPattern;
    uint64_t Updates;
    uint64_t PrevAddress;
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

    AddressPatternFSM(llvm::Instruction *_BaseLoad)
        : CurrentPattern(UNKNOWN), Updates(0), PrevAddress(0),
          BaseLoad(_BaseLoad), Base(0), StrideI(0), I(0), NI(0), StrideJ(0),
          J(0) {
      if (this->BaseLoad != nullptr) {
        this->CurrentPattern = INDIRECT;
      }
    }

    void update(uint64_t Addr);
    void updateMissing();
  };

  class AccessPatternFSM {
  public:
    enum StateT {
      UNKNOWN,
      SUCCESS,
      FAILURE,
    };

    // Statistics.
    uint64_t Accesses;
    uint64_t Iters;

    void addAccess(uint64_t Addr);
    void addMissingAccess();

    StateT getState() const { return this->State; }

    const AddressPatternFSM &getAddressPattern() const {
      return this->AddressPattern;
    }

    AccessPattern getAccessPattern() const { return this->AccPattern; }

    AccessPatternFSM(AccessPattern _AccPattern, llvm::Instruction *_BaseLoad)
        : Accesses(0), Iters(0), State(UNKNOWN), AccPattern(_AccPattern),
          AddressPattern(_BaseLoad) {}

  protected:
    StateT State;

    const AccessPattern AccPattern;

    AddressPatternFSM AddressPattern;
  };

  llvm::Instruction *MemInst;

  /**
   * Representing an ongoing computation.
   */
  std::vector<AccessPatternFSM *> ComputingFSMs;

  static llvm::Instruction *isIndirect(llvm::Instruction *Inst);

  /**
   * Implement the address pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A)
   */
  static bool isAddressPatternRelaxed(Pattern A, Pattern B);

  /**
   * Implement the access pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A).
   */
  static bool isAccessPatternRelaxed(AccessPattern A, AccessPattern B);

public:
  struct ComputedPattern {
    Pattern CurrentPattern;
    AccessPattern AccPattern;
    uint64_t Accesses;
    uint64_t Updates;
    uint64_t Iters;
    uint64_t StreamCount;
    // For indirect memory access.
    llvm::Instruction *BaseLoad;
    ComputedPattern(const AccessPatternFSM &NewFSM)
        : CurrentPattern(NewFSM.getAddressPattern().CurrentPattern),
          AccPattern(NewFSM.getAccessPattern()), Accesses(NewFSM.Accesses),
          Updates(NewFSM.getAddressPattern().Updates), Iters(NewFSM.Iters),
          StreamCount(1), BaseLoad(NewFSM.getAddressPattern().BaseLoad) {}

    void merge(const AccessPatternFSM &NewFSM);
  };

  const ComputedPattern &getPattern() const;

private:
  ComputedPattern *ComputedPatternPtr;
};

#endif