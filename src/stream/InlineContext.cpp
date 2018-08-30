#include "stream/InlineContext.h"
#include "Utils.h"

#include <sstream>

InlineContext::InlineContext(const InlineContext &Other)
    : Context(Other.Context) {}

InlineContext &InlineContext::operator=(const InlineContext &Other) {
  if (&Other == this) {
    return *this;
  }
  this->Context = Other.Context;
  return *this;
}

bool InlineContext::operator==(const InlineContext &Other) const {
  if (&Other == this) {
    return true;
  }

  if (this->Context.size() != Other.Context.size()) {
    return false;
  }

  auto ThisIter = this->Context.begin();
  auto OtherIter = Other.Context.begin();

  while (ThisIter != this->Context.end() && OtherIter != Other.Context.end()) {
    if ((*ThisIter) != (*OtherIter)) {
      return false;
    }
    ThisIter++;
    OtherIter++;
  }

  return true;
}

bool InlineContext::contains(const InlineContext& Other) const {
  if (&Other == this) {
    return true;
  }
  if (this->Context.size() > Other.Context.size()) {
    return false;
  }
  auto ThisIter = this->Context.begin();
  auto OtherIter = Other.Context.begin();

  while (ThisIter != this->Context.end() && OtherIter != Other.Context.end()) {
    if ((*ThisIter) != (*OtherIter)) {
      return false;
    }
    ++ThisIter;
    ++OtherIter;
  }

  return ThisIter == this->Context.end();
}

std::string InlineContext::format() const {
  std::stringstream ss;
  ss << "Main";
  for (auto Inst : this->Context) {
    ss << "->" << LoopUtils::formatLLVMInst(Inst);
  }
  return ss.str();
}

void InlineContext::push(llvm::Instruction *Inst) {
  assert(Utils::isCallOrInvokeInst(Inst) &&
         "Only call/invoke instructions are allowed in the InlineContext");

  this->Context.push_back(Inst);
}

void InlineContext::pop() {
  assert(!this->Context.empty() && "Pop from empty context.");
  this->Context.pop_back();
}

bool InlineContext::isRecursive(llvm::Function *Func) const {
  auto Iter = this->Context.begin();
  // Ignore the top(last) one as it is the current function.
  for (int i = 0; i + 1 < this->Context.size(); ++i, ++Iter) {
    auto Inst = *Iter;
    if (Inst->getFunction() == Func) {
      return true;
    }
  }
  return false;
}

bool ContextLoop::contains(const ContextLoop& Loop) const {
  if (this->Context == Loop.Context) {
    return this->Loop->contains(Loop.Loop);
  }
  return this->Context.contains(Loop.Context);
}

bool ContextLoop::contains(const ContextInst &Inst) const {
  if (this->Context == Inst.Context) {
    return this->Loop->contains(Inst.Inst);
  }
  return this->Context.contains(Inst.Context);
}