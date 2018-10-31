
#include "StreamPassTestFixture.h"

TEST_F(StreamTransformPassTestFixture, MultipleSteps) {
  this->setUpEnvironment(GetTestInputSource("/stream/inputs/Unrolled.c"));

  this->Pass.runOnModule(*(this->Module));

  const auto &InstTransformPlanMap = this->Pass.getInstTransformPlanMap();

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  std::vector<PlanT> ExpectedTransformPlanTypes = {
      PlanT::DELETE,  // phi
      PlanT::DELETE,  // phi
      PlanT::DELETE,  // phi
      PlanT::NOTHING, // gep
      PlanT::STORE,   // store
      PlanT::DELETE,  // bitcast
      PlanT::STEP,    // gep
      PlanT::STORE,   // store
      PlanT::DELETE,  // bitcast
      PlanT::STEP,    // add
      PlanT::NOTHING, // icmp
      PlanT::NOTHING, // br
  };

  std::unordered_map<std::string, std::unordered_set<std::string>>
      ExpectedStepStreamsMap = {
          {"tmp13", {"tmp6"}},
          {"tmp11", {"tmp7", "tmp8"}},
      };

  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    if (BBIter->getName() != "bb5") {
      continue;
    }

    int InstIdx = 0;
    for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
         InstIter != InstEnd; ++InstIter, ++InstIdx) {
      auto Inst = &*InstIter;
      auto PlanIter = InstTransformPlanMap.find(Inst);
      EXPECT_NE(InstTransformPlanMap.end(), PlanIter)
          << "Missing transform plan from the map for inst "
          << Utils::formatLLVMInst(Inst) << '\n';
      EXPECT_EQ(ExpectedTransformPlanTypes[InstIdx], PlanIter->second.Plan)
          << "Mismatch transformation plan for inst "
          << Utils::formatLLVMInst(Inst) << '\n';

      std::string InstName = Inst->getName();
      if (ExpectedStepStreamsMap.count(InstName) == 0) {
        continue;
      }

      if (PlanIter->second.Plan == PlanT::STEP) {
        std::unordered_set<std::string> ActualStepStreams;
        for (const auto &StepStream : PlanIter->second.getStepStreams()) {
          ActualStepStreams.insert(StepStream->getInst()->getName());
        }
        this->testTwoSets<std::string>(
            ExpectedStepStreamsMap.at(InstName), ActualStepStreams,
            [](const std::string &E) -> std::string { return E; });
      }
    }
    EXPECT_EQ(ExpectedTransformPlanTypes.size(), InstIdx)
        << "Mismatch number of instructions.\n";
  }
}