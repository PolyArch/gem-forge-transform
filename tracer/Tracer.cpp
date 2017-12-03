#include "Tracer.hpp"

#include <fstream>
#include <iostream>

#define DEBUG_TRACE_PARSER

#ifdef DEBUG_TRACE_PARSER
#include <cstdio>
#define DEBUG(...) printf(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

namespace tracer {

std::shared_ptr<Function> Trace::getOrCreateFunction(const std::string& Name) {
  if (FunctionsMap.find(Name) == FunctionsMap.end()) {
    auto Func = std::make_shared<Function>(Name);
    FunctionsMap.emplace(std::piecewise_construct, std::forward_as_tuple(Name),
                         std::forward_as_tuple(Func));
  }
  return FunctionsMap[Name];
}

std::shared_ptr<Trace> Trace::parse(const std::string& FileName) {
  DEBUG("Parse trace file: %s\n", FileName.c_str());
  std::ifstream TraceStream(FileName);
  if (!TraceStream.is_open()) {
    std::cerr << "Failed open file: " << FileName << std::endl;
    return nullptr;
  }

  auto RetTrace = std::make_shared<Trace>();

  std::string Line;
  while (std::getline(TraceStream, Line)) {
    DEBUG("Read in line: %s\n", Line.c_str());
    size_t Start = 0;
    std::vector<std::string> Fields;
    for (auto End = Line.find(',', Start); End != std::string::npos;
         Start = End + 1, End = Line.find(',', Start)) {
      Fields.push_back(Line.substr(Start, End - Start));
    }

    auto Func = RetTrace->getOrCreateFunction(Fields[0]);
    auto BB = Func->getOrCreateBasicBlock(Fields[1]);

    auto Id = std::stoul(Fields[2]);
    auto StaticInst = BB->getInstruction(Id);
    if (!StaticInst) {
        auto OpCodeName = Fields[3];
        std::set<std::string> Operands(Fields.begin() + 5, Fields.end());
        StaticInst = std::make_shared<StaticInstruction>(Id, OpCodeName, std::move(Operands));
        BB->insertInstruction(StaticInst);
        DEBUG("Created a static inst: %s\n", StaticInst->OpCodeName.c_str());
    }

    // Create the dynamic instructions.
    DEBUG("Pushed a dynamic inst\n");
    auto DynamicInstId = RetTrace->Dynamics.size();
    auto DynamicInst = std::make_shared<DynamicInstruction>();
    DynamicInst->Id = DynamicInstId;
    DynamicInst->Static = StaticInst;
    RetTrace->Dynamics.push_back(DynamicInst);
  }
  return nullptr;
}
}  // namespace tracer

int main(int argc, char* argv[]) {
  std::string FileName(argv[1]);
  auto Trace = tracer::Trace::parse(FileName);
  return 0;
}