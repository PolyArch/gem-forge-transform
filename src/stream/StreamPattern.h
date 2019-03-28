#ifndef LLVM_TDG_MEMORY_ACCESS_PATTERN_H
#define LLVM_TDG_MEMORY_ACCESS_PATTERN_H

#include <list>
#include <unordered_map>
#include <vector>

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
class StreamPattern {
public:
  /**
   */
  enum ValuePattern {
    UNKNOWN,
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

  /**
   * Represents a computed pattern for the previous instance of stream.
   */
  struct ComputedPattern {
    ValuePattern ValPattern;
    AccessPattern AccPattern;
    uint64_t Accesses;
    uint64_t Updates;
    uint64_t Iters;
    uint64_t Base;
    int64_t StrideI;
    uint64_t NI;
    int64_t StrideJ;
    const std::vector<std::pair<bool, uint64_t>> History;
    ComputedPattern(std::vector<std::pair<bool, uint64_t>> _History)
        : ValPattern(ValuePattern::UNKNOWN),
          AccPattern(AccessPattern::UNCONDITIONAL), Base(0), StrideI(0), NI(0),
          StrideJ(0), History(std::move(_History)) {}
  };

  StreamPattern() : AggregatedPatternPtr(nullptr) {}
  ~StreamPattern() {
    if (this->AggregatedPatternPtr != nullptr) {
      delete this->AggregatedPatternPtr;
      this->AggregatedPatternPtr = nullptr;
    }
  }
  StreamPattern(const StreamPattern &Other) = delete;
  StreamPattern(StreamPattern &&Other) = delete;
  StreamPattern &operator=(const StreamPattern &Other) = delete;
  StreamPattern &operator=(StreamPattern &&Other) = delete;

  /**
   * Add one dynamic access address for a specific instruction.
   */
  void addAccess(uint64_t Val);

  /**
   * There is one missing access in this iteration.
   * We implicitly assume the missing access complys with the current pattern.
   */
  void addMissingAccess();

  /**
   * Wraps up an ongoing stream.
   */
  const ComputedPattern endStream();

  /**
   * Finalize the pattern computed.
   * This will compute the average length of the stream and if
   * that is lower than a threshold, we think this is random pattern?
   */
  void finalizePattern();

  bool computed() const;

  static std::string formatValuePattern(ValuePattern ValPattern);
  static std::string formatAccessPattern(AccessPattern AccPattern);

private:
  class ValuePatternFSM {
  public:
    enum StateT {
      UNKNOWN,
      SUCCESS,
      FAILURE,
    };

    StateT State;

    uint64_t Updates;
    uint64_t PrevValue;

    ValuePatternFSM() : State(UNKNOWN), Updates(0), PrevValue(0) {}
    virtual ~ValuePatternFSM() = default;

    virtual ValuePattern getValuePattern() const = 0;
    virtual void update(uint64_t Val) = 0;
    virtual void updateMissing() = 0;
    virtual void fillInComputedPattern(ComputedPattern &OutPattern) const {
      // Default implementation will simply fill in the ValuePattern;
      OutPattern.ValPattern = this->getValuePattern();
    }

    /**
     * Helper function to compute the linear stride.
     * The result is valid iff. the 3rd field is true.
     */
    static std::pair<bool, std::pair<uint64_t, int64_t>>
    computeLinearBaseStride(const std::pair<uint64_t, uint64_t> &Record0,
                            const std::pair<uint64_t, uint64_t> &Record1) {
      auto Idx0 = Record0.first;
      auto Val0 = Record0.second;
      auto Idx1 = Record1.first;
      auto Val1 = Record1.second;
      auto ValDiff = Val1 - Val0;
      auto IdxDiff = Idx1 - Idx0;
      if (ValDiff == 0 || IdxDiff == 0) {
        return std::make_pair(false, std::make_pair(static_cast<uint64_t>(0),
                                                    static_cast<int64_t>(0)));
      }
      if (ValDiff % IdxDiff != 0) {
        return std::make_pair(false, std::make_pair(static_cast<uint64_t>(0),
                                                    static_cast<int64_t>(0)));
      }

      int64_t Stride = ValDiff / IdxDiff;
      return std::make_pair(
          true, std::make_pair(static_cast<uint64_t>(Val0 - Idx0 * Stride),
                               static_cast<int64_t>(Stride)));
    }
  };

  class UnknownValuePatternFSM : public ValuePatternFSM {
  public:
    UnknownValuePatternFSM() : ValuePatternFSM() {
      // Unknown starts as success.
      this->State = SUCCESS;
    }
    void update(uint64_t Val) override;
    void updateMissing() override;
    ValuePattern getValuePattern() const override {
      return ValuePattern::UNKNOWN;
    }
  };

  class ConstValuePatternFSM : public ValuePatternFSM {
  public:
    ConstValuePatternFSM() : ValuePatternFSM() {}
    void update(uint64_t Val) override;
    void updateMissing() override;
    ValuePattern getValuePattern() const override {
      return ValuePattern::CONSTANT;
    }
    void fillInComputedPattern(ComputedPattern &OutPattern) const override;
  };

  class LinearValuePatternFSM : public ValuePatternFSM {
  public:
    LinearValuePatternFSM() : ValuePatternFSM(), Base(0), Stride(0), I(0) {}
    void update(uint64_t Val) override;
    void updateMissing() override;
    ValuePattern getValuePattern() const override {
      return ValuePattern::LINEAR;
    }
    void fillInComputedPattern(ComputedPattern &OutPattern) const override;

  private:
    uint64_t Base;
    int64_t Stride;
    uint64_t I;
    std::list<std::pair<uint64_t, uint64_t>> ConfirmedVals;
    uint64_t computeNextVal() const;
  };

  class QuardricValuePatternFSM : public ValuePatternFSM {
  public:
    QuardricValuePatternFSM()
        : ValuePatternFSM(), Base(0), StrideI(0), I(0), NI(0), StrideJ(0),
          J(0) {
      // For now always failed quardric pattern.
      // this->State == FAILURE;
    }
    void update(uint64_t Val) override;
    void updateMissing() override;
    ValuePattern getValuePattern() const override {
      return ValuePattern::QUARDRIC;
    }
    void fillInComputedPattern(ComputedPattern &OutPattern) const override;

  private:
    uint64_t Base;
    int64_t StrideI;
    uint64_t I;
    uint64_t NI;
    int64_t StrideJ;
    uint64_t J;
    std::list<std::pair<uint64_t, uint64_t>> ConfirmedVals;

    // Helper function to check if the 3 confirmed address is actually aligned
    // on the same line.
    bool isAligned() const;

    // Compute the next address.
    uint64_t computeNextVal() const;

    // Step the I, J to the next iteration.
    void step();
  };

  class RandomValuePatternFSM : public ValuePatternFSM {
  public:
    RandomValuePatternFSM() : ValuePatternFSM() {
      // Always success for random pattern.
      this->State = SUCCESS;
    }
    void update(uint64_t Val) override;
    void updateMissing() override;
    ValuePattern getValuePattern() const override {
      return ValuePattern::RANDOM;
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

    void addAccess(uint64_t Val);
    void addMissingAccess();

    StateT getState() const { return this->State; }

    const ValuePatternFSM &getValuePatternFSM() const;

    AccessPattern getAccessPattern() const { return this->AccPattern; }

    AccessPatternFSM(AccessPattern _AccPattern);
    ~AccessPatternFSM();

  protected:
    StateT State;

    const AccessPattern AccPattern;

    std::list<ValuePatternFSM *> ValuePatterns;

    void feedUpdateMissingToValPatterns() {
      for (auto &ValPattern : this->ValuePatterns) {
        ValPattern->updateMissing();
      }
    }
    void feedUpdateToValPatterns(uint64_t Val) {
      for (auto &ValPattern : this->ValuePatterns) {
        ValPattern->update(Val);
      }
    }
  };

  /**
   * Representing an ongoing computation.
   */
  std::vector<AccessPatternFSM *> ComputingFSMs;

  std::vector<std::pair<bool, uint64_t>> History;

  /**
   * Implement the address pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A)
   */
  static bool isValuePatternRelaxed(ValuePattern A, ValuePattern B);

  /**
   * Implement the access pattern hierarchy.
   * Returns true if B is more relaxed than A. (B >= A).
   */
  static bool isAccessPatternRelaxed(AccessPattern A, AccessPattern B);

public:
  struct AggregatedPattern {
    ValuePattern ValPattern;
    AccessPattern AccPattern;
    uint64_t Accesses;
    uint64_t Updates;
    uint64_t Iters;
    uint64_t StreamCount;
    AggregatedPattern(const AccessPatternFSM &NewFSM)
        : ValPattern(NewFSM.getValuePatternFSM().getValuePattern()),
          AccPattern(NewFSM.getAccessPattern()), Accesses(NewFSM.Accesses),
          Updates(NewFSM.getValuePatternFSM().Updates), Iters(NewFSM.Iters),
          StreamCount(1) {}

    AggregatedPattern() {}

    void merge(const AccessPatternFSM &NewFSM);
  };

  const AggregatedPattern &getPattern() const;

private:
  AggregatedPattern *AggregatedPatternPtr;

  void initialize();
};

#endif
