#ifndef LLVM_TDG_INLINE_CONTEXT_H
#define LLVM_TDG_INLINE_CONTEXT_H

#include "LoopUtils.h"

#include "llvm/IR/Instruction.h"

#include <list>
/**
 * InlineContext will be used in a lot of places, and to optimize it we use a
 * tree and only expose the pointer to user.
 *
 * The context is allocated but never freed.
 */

struct InlineContext;

using InlineContextPtr = const InlineContext *;

class InlineContext {
public:
  static InlineContextPtr getEmptyContext();

  bool contains(const InlineContext &Other) const;

  InlineContextPtr push(llvm::Instruction *Inst) const;
  InlineContextPtr pop() const;

  bool empty() const { return this->Context.empty(); }
  size_t size() const { return this->Context.size(); }

  /**
   * Detects if Func is already in the context, which means that Func is
   * recursive.
   */
  bool isRecursive(llvm::Function *Func) const;

  std::string format() const;
  std::string beautify() const;

  const std::list<llvm::Instruction *> Context;

private:
  InlineContext();
  InlineContext(InlineContextPtr _Parent,
                std::list<llvm::Instruction *> &&_Context);
  InlineContext(const InlineContext &Other) = delete;
  InlineContext(InlineContext &&Other) = delete;

  InlineContext &operator=(const InlineContext &Other) = delete;
  InlineContext &operator=(InlineContext &&Other) = delete;

  bool operator==(const InlineContext &Other) const;
  bool operator!=(const InlineContext &Other) const {
    return !(this->operator==(Other));
  }

  mutable std::unordered_map<llvm::Instruction *, InlineContextPtr> Children;
  const InlineContextPtr Parent;

  static InlineContext EmptyRoot;
};

struct ContextInst {
public:
  ContextInst(const InlineContextPtr &_Context, llvm::Instruction *_Inst)
      : Context(_Context), Inst(_Inst) {
    assert(this->Inst != nullptr &&
           "Inst is nullptr when constructing ContextInst.");
    assert(this->Context != nullptr &&
           "Context is nullptr when constructing ContextInst.");
  }

  ContextInst(const ContextInst &Other)
      : Context(Other.Context), Inst(Other.Inst) {
    assert(this->Inst != nullptr &&
           "Inst is nullptr when constructing ContextInst.");
    assert(this->Context != nullptr &&
           "Context is nullptr when constructing ContextInst.");
  }
  ContextInst &operator=(const ContextInst &Other) = delete;
  ContextInst(ContextInst &&Other) = delete;
  ContextInst &operator=(ContextInst &&Other) = delete;

  bool operator==(const ContextInst &Other) const {
    if (this == &Other) {
      return true;
    }
    return this->Context == Other.Context && this->Inst == Other.Inst;
  }

  bool operator!=(const ContextInst &Other) const {
    return !(this->operator==(Other));
  }

  std::string format() const {
    assert(this->Inst != nullptr &&
           "Inst is nullptr when formating ContextInst.");
    return this->Context->format() + "->" +
           LoopUtils::formatLLVMInst(this->Inst);
  }

  std::string beautify() const {
    return this->Context->beautify() + "->" +
           LoopUtils::formatLLVMInst(this->Inst);
  }

  const InlineContextPtr Context;
  const llvm::Instruction *const Inst;
};

struct ContextLoop {
public:
  ContextLoop(const InlineContextPtr &_Context, llvm::Loop *_Loop)
      : Context(_Context), Loop(_Loop) {}
  ContextLoop(const ContextLoop &Other)
      : Context(Other.Context), Loop(Other.Loop) {}
  ContextLoop(ContextLoop &&Other) = delete;
  ContextLoop &operator=(ContextLoop &Other) = delete;
  // ContextLoop &operator=(const ContextLoop &Other) {
  //   if (this == &Other) {
  //     return *this;
  //   }
  //   this->Context = Other.Context;
  //   this->Loop = Other.Loop;
  //   return *this;
  // }
  ContextLoop &operator=(ContextLoop &&Other) = delete;

  bool operator==(const ContextLoop &Other) const {
    if (this == &Other) {
      return true;
    }
    return this->Context == Other.Context && this->Loop == Loop;
  }

  bool operator!=(const ContextLoop &Other) const {
    return !(this->operator==(Other));
  }

  bool contains(const ContextLoop &Loop) const;
  bool contains(const ContextInst &Inst) const;

  std::string format() const {
    return this->Context->format() + "->" + LoopUtils::getLoopId(this->Loop);
  }

  std::string beautify() const {
    return this->Context->beautify() + "->" + LoopUtils::getLoopId(this->Loop);
  }

  const InlineContextPtr Context;
  const llvm::Loop *Loop;
};

// Our own hash function.
namespace std {
/**
 * Normally you can not hash pointer as it violates the semantic.
 * However here I abuse it as I know for sure that the they are unique per
 * address.
 */
// template <> struct hash<InlineContext> {
//   std::size_t operator()(const InlineContext &Context) const {

//     size_t Result = 0xabcdef01;
//     for (const auto &Inst : Context.Context) {
//       Result >>= 1;
//       Result ^= (hash<uint64_t>()(reinterpret_cast<uint64_t>(Inst)) << 1);
//     }

//     return Result;
//   }
// };

template <> struct hash<ContextInst> {
  std::size_t operator()(const ContextInst &Inst) const {
    using std::hash;
    using std::size_t;

    return hash<uint64_t>()(reinterpret_cast<uint64_t>(Inst.Context)) ^
           (hash<uint64_t>()(reinterpret_cast<uint64_t>(Inst.Inst)) << 1);
  }
};

template <> struct hash<ContextLoop> {
  std::size_t operator()(const ContextLoop &Loop) const {
    using std::hash;
    using std::size_t;

    return hash<uint64_t>()(reinterpret_cast<uint64_t>(Loop.Context)) ^
           (hash<uint64_t>()(reinterpret_cast<uint64_t>(Loop.Loop)) << 1);
  }
};

} // namespace std

#endif