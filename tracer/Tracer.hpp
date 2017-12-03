#ifndef TRACER_TRACER_HH
#define TRACER_TRACER_HH

#include "Static.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

namespace tracer {
// Represent an instruction in the trace.
struct DynamicInstruction {
  unsigned Id;
  std::shared_ptr<StaticInstruction> Static;
  std::vector<std::shared_ptr<DynamicInstruction>> Dependence;
};
struct Trace {
  std::vector<std::shared_ptr<DynamicInstruction>> Dynamics;
  std::unordered_map<std::string, std::shared_ptr<Function>> FunctionsMap;

  static std::shared_ptr<Trace> parse(const std::string& FileName);
  
  std::shared_ptr<Function> getOrCreateFunction(const std::string& Name);
};
}  // namespace tracer
#endif