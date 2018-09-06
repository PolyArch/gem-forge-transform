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
class MemoryPattern {
public:
  /**
   */
  enum AddressPattern {
    UNKNOWN,
    CONSTANT,
    LINEAR,
    QUARDRIC,
    RANDOM,
  };

  MemoryPattern(const llvm::Instruction *_MemInst)
      : MemInst(_MemInst), ComputedPatternPtr(nullptr) {}
  ~MemoryPattern() {
    if (this->ComputedPatternPtr != nullptr) {
      delete this->ComputedPatternPtr;
      this->ComputedPatternPtr = nullptr;
    }
  }
  MemoryPattern(const MemoryPattern &Other) = delete;
  MemoryPattern(MemoryPattern &&Other) = delete;
  MemoryPattern &operator=(const MemoryPattern &Other) = delete;
  MemoryPattern &operator=(MemoryPattern &&Other) = delete;

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

  static std::string formatAddressPattern(AddressPattern AddrPattern);
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

    uint64_t Updates;
    uint64_t PrevAddress;

    AddressPatternFSM() : State(UNKNOWN), Updates(0), PrevAddress(0) {}

    virtual AddressPattern getAddressPattern() const = 0;
    virtual void update(uint64_t Addr) = 0;
    virtual void updateMissing() = 0;

    // Some helper functions
    static std::pair<uint64_t, int64_t>
    computeLinearBaseStride(const std::pair<uint64_t, uint64_t> &Record0,
                            const std::pair<uint64_t, uint64_t> &Record1) {
      auto Idx0 = Record0.first;
      auto Addr0 = Record0.second;
      auto Idx1 = Record1.first;
      auto Addr1 = Record1.second;
      auto AddrDiff = Addr1 - Addr0;
      auto IdxDiff = Idx1 - Idx0;
      if (AddrDiff == 0 || IdxDiff == 0) {
        return std::make_pair(static_cast<uint64_t>(0),
                              static_cast<int64_t>(0));
      }
      if (AddrDiff % IdxDiff != 0) {
        return std::make_pair(static_cast<uint64_t>(0),
                              static_cast<int64_t>(0));
      }

      int64_t Stride = AddrDiff / IdxDiff;
      return std::make_pair(static_cast<uint64_t>(Addr0 - Idx0 * Stride),
                            Stride);
    }
  };

  class UnknownAddressPatternFSM : public AddressPatternFSM {
  public:
    UnknownAddressPatternFSM() : AddressPatternFSM() {
      // Unknown starts as success.
      this->State = SUCCESS;
    }
    void update(uint64_t Addr) override;
    void updateMissing() override;
    AddressPattern getAddressPattern() const override {
      return AddressPattern::UNKNOWN;
    }
  };

  class ConstAddressPatternFSM : public AddressPatternFSM {
  public:
    ConstAddressPatternFSM() : AddressPatternFSM() {}
    void update(uint64_t Addr) override;
    void updateMissing() override;
    AddressPattern getAddressPattern() const override {
      return AddressPattern::CONSTANT;
    }
  };

  class LinearAddressPatternFSM : public AddressPatternFSM {
  public:
    LinearAddressPatternFSM() : AddressPatternFSM(), Base(0), Stride(0), I(0) {}
    void update(uint64_t Addr) override;
    void updateMissing() override;
    AddressPattern getAddressPattern() const override {
      return AddressPattern::LINEAR;
    }

  private:
    uint64_t Base;
    int64_t Stride;
    uint64_t I;
    std::list<std::pair<uint64_t, uint64_t>> ConfirmedAddrs;
    uint64_t computeNextAddr() const;
  };

  class QuardricAddressPatternFSM : public AddressPatternFSM {
  public:
    QuardricAddressPatternFSM()
        : AddressPatternFSM(), Base(0), StrideI(0), I(0), NI(0), StrideJ(0),
          J(0) {
      // For now always failed quardric pattern.
      // this->State == FAILURE;
    }
    void update(uint64_t Addr) override;
    void updateMissing() override;
    AddressPattern getAddressPattern() const override {
      return AddressPattern::QUARDRIC;
    }

  private:
    uint64_t Base;
    int64_t StrideI;
    uint64_t I;
    uint64_t NI;
    int64_t StrideJ;
    uint64_t J;
    std::list<std::pair<uint64_t, uint64_t>> ConfirmedAddrs;

    // Helper function to check if the 3 confirmed address is actually aligned
    // on the same line.
    bool isAligned() const;

    // Compute the next address.
    uint64_t computeNextAddr() const;

    // Step the I, J to the next iteration.
    void step();
  };

  class RandomAddressPatternFSM : public AddressPatternFSM {
  public:
    RandomAddressPatternFSM() : AddressPatternFSM() {
      // Always success for random pattern.
      this->State = SUCCESS;
    }
    void update(uint64_t Addr) override;
    void updateMissing() override;
    AddressPattern getAddressPattern() const override {
      return AddressPattern::RANDOM;
    }
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

    const AddressPatternFSM &getAddressPatternFSM() const;

    AccessPattern getAccessPattern() const { return this->AccPattern; }

    AccessPatternFSM(AccessPattern _AccPattern);
    ~AccessPatternFSM();

  protected:
    StateT State;

    const AccessPattern AccPattern;

    std::list<AddressPatternFSM *> AddressPatterns;

    void feedUpdateMissingToAddrPatterns() {
      for (auto &AddrPattern : this->AddressPatterns) {
        AddrPattern->updateMissing();
      }
    }
    void feedUpdateToAddrPatterns(uint64_t Addr) {
      for (auto &AddrPattern : this->AddressPatterns) {
        AddrPattern->update(Addr);
      }
    }
  };

  const llvm::Instruction *MemInst;

  MemoryFootprint Footprint;

  /**
   * Representing an ongoing computation.
   */
  std::vector<AccessPatternFSM *> ComputingFSMs;

  /**
   * Implement the address pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A)
   */
  static bool isAddressPatternRelaxed(AddressPattern A, AddressPattern B);

  /**
   * Implement the access pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A).
   */
  static bool isAccessPatternRelaxed(AccessPattern A, AccessPattern B);

public:
  struct ComputedPattern {
    AddressPattern AddrPattern;
    AccessPattern AccPattern;
    uint64_t Accesses;
    uint64_t Updates;
    uint64_t Iters;
    uint64_t StreamCount;
    ComputedPattern(const AccessPatternFSM &NewFSM)
        : AddrPattern(NewFSM.getAddressPatternFSM().getAddressPattern()),
          AccPattern(NewFSM.getAccessPattern()), Accesses(NewFSM.Accesses),
          Updates(NewFSM.getAddressPatternFSM().Updates), Iters(NewFSM.Iters),
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