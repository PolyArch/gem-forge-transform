
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
      PlanT::DELETE,  // bitcast
      PlanT::DELETE,  // gep
      PlanT::STORE,   // store
      PlanT::DELETE,  // bitcast
      PlanT::STEP,    // gep
      PlanT::STORE,   // store
      PlanT::STEP,    // add
      PlanT::NOTHING, // icmp
      PlanT::NOTHING, // br
  };

  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    if (BBIter->getName() != "bb4") {
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
    }
  }
}