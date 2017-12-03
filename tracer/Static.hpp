#ifndef TRACER_STATIC_HH
#define TRACER_STATIC_HH
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace tracer {
struct StaticInstruction {
  const unsigned Id;
  const std::string OpCodeName;
  const std::set<std::string> Operands;
  StaticInstruction(const unsigned _Id, const std::string& _OpCodeName,
                    std::set<std::string> _Operands)
      : Id(_Id), OpCodeName(_OpCodeName), Operands(std::move(_Operands)) {}
};
class BasicBlock {
 public:
  BasicBlock(const std::string& _Name) : Name(_Name) {}

  bool hasInstruction(unsigned int Id) {
    return Instructions.find(Id) != Instructions.end();
  }

  std::shared_ptr<StaticInstruction> getInstruction(unsigned int Id) {
    if (Instructions.find(Id) == Instructions.end()) {
      return nullptr;
    } else {
      return Instructions[Id];
    }
  }

  void insertInstruction(std::shared_ptr<StaticInstruction> Instruction) {
    Instructions.emplace(std::piecewise_construct,
                         std::forward_as_tuple(Instruction->Id),
                         std::forward_as_tuple(Instruction));
  }

 private:
  const std::string Name;
  std::map<unsigned, std::shared_ptr<StaticInstruction>> Instructions;
};
class Function {
 public:
  Function(const std::string& _Name) : Name(_Name) {}

  std::shared_ptr<BasicBlock> getOrCreateBasicBlock(const std::string& Name) {
    if (BasicBlockMap.find(Name) == BasicBlockMap.end()) {
      auto BB = std::make_shared<BasicBlock>(Name);
      BasicBlockMap.emplace(std::piecewise_construct,
                            std::forward_as_tuple(Name),
                            std::forward_as_tuple(BB));
    }
    return BasicBlockMap[Name];
  }

 private:
  const std::string Name;
  std::unordered_map<std::string, std::shared_ptr<BasicBlock>> BasicBlockMap;
};
}  // namespace tracer
#endif