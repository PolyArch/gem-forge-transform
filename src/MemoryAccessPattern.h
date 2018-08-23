#ifndef LLVM_TDG_MEMORY_ACCESS_PATTERN_H
#define LLVM_TDG_MEMORY_ACCESS_PATTERN_H

#include "MemoryFootprint.h"

#include "llvm/IR/Instruction.h"

#include <list>
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
 *
 * The address pattern starts from UNKNOWN, representing that there is no memory
 * access seen so far. As the user provides more accessed address, we gradually
 * relax our constraints to RANDOM.
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
  /**
   */
  enum Pattern {
    UNKNOWN,
    CONSTANT,
    LINEAR,
    QUARDRIC,
    RANDOM,
  };

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

  const MemoryFootprint &getFootprint() const { return this->Footprint; }

  bool computed() const;

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
    enum StateT {
      UNKNOWN,
      SUCCESS,
      FAILURE,
    };

    StateT State;

    Pattern CurrentPattern;
    uint64_t Updates;
    uint64_t PrevAddress;
    // CONSTANT.
    uint64_t Base;
    // LINEAR.
    int64_t StrideI;
    uint64_t I;
    // QUARDRIC.
    uint64_t NI;
    int64_t StrideJ;
    uint64_t J;

    AddressPatternFSM(Pattern _CurrentPattern);

    void update(uint64_t Addr);
    void updateMissing();

  private:
    std::list<std::pair<uint64_t, uint64_t>> ConfirmedAddrs;
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

    const AddressPatternFSM &getAddressPattern() const;

    AccessPattern getAccessPattern() const { return this->AccPattern; }

    AccessPatternFSM(AccessPattern _AccPattern);

  protected:
    StateT State;

    const AccessPattern AccPattern;

    std::list<AddressPatternFSM> AddressPatterns;

    void feedUpdateMissingToAddrPatterns() {
      for (auto &AddrPattern : this->AddressPatterns) {
        AddrPattern.updateMissing();
      }
    }
    void feedUpdateToAddrPatterns(uint64_t Addr) {
      for (auto &AddrPattern : this->AddressPatterns) {
        AddrPattern.update(Addr);
      }
    }
  };

  llvm::Instruction *MemInst;

  MemoryFootprint Footprint;

  /**
   * Representing an ongoing computation.
   */
  std::vector<AccessPatternFSM *> ComputingFSMs;

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
    ComputedPattern(const AccessPatternFSM &NewFSM)
        : CurrentPattern(NewFSM.getAddressPattern().CurrentPattern),
          AccPattern(NewFSM.getAccessPattern()), Accesses(NewFSM.Accesses),
          Updates(NewFSM.getAddressPattern().Updates), Iters(NewFSM.Iters),
          StreamCount(1) {}

    ComputedPattern() {}

    void merge(const AccessPatternFSM &NewFSM);
  };

  const ComputedPattern &getPattern() const;

private:
  ComputedPattern *ComputedPatternPtr;

  void initialize();
};

#endif