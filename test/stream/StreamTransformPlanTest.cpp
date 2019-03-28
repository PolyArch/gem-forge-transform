
#include "StreamPassTestFixture.h"

namespace {
class StreamTransformPassTransformPlanTestFixture
    : public StreamTransformPassTestFixture {
protected:
  using PlanT = StreamTransformPlan::PlanT;
  using ExpectedPlanMapT = std::unordered_map<std::string, std::vector<PlanT>>;
  using ExpectedStepStreamMapT =
      std::unordered_map<std::string, std::unordered_set<std::string>>;

  void testTransformPlan(const llvm::Loop *Loop,
                         const ExpectedPlanMapT &ExpectedPlan,
                         const ExpectedStepStreamMapT &ExpectedStepStreams) {

    auto Analyzer = this->Pass.getAnalyzerByLoop(Loop);
    EXPECT_NE(nullptr, Analyzer) << "Failed to find the stream analyzer.\n";
    const auto &InstTransformPlanMap = Analyzer->getInstTransformPlanMap();

    for (auto BBIter = Loop->block_begin(), BBEnd = Loop->block_end();
         BBIter != BBEnd; ++BBIter) {
      auto BB = *BBIter;
      auto BBName = BB->getName();
      auto ExpectedPlanIter = ExpectedPlan.find(BBName);
      if (ExpectedPlanIter == ExpectedPlan.end()) {
        // We do not care about this block.
        continue;
      }
      const auto &ExpectedPlans = ExpectedPlanIter->second;
      size_t InstIdx = 0;
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter, ++InstIdx) {
        auto Inst = &*InstIter;

        auto ActualPlanIter = InstTransformPlanMap.find(Inst);
        EXPECT_NE(InstTransformPlanMap.end(), ActualPlanIter)
            << "Missing transform plan from the map for inst "
            << Utils::formatLLVMInst(Inst) << '\n';

        const auto &ActualPlan = ActualPlanIter->second;
        EXPECT_EQ(ExpectedPlans[InstIdx], ActualPlan.Plan)
            << "Mismatch transformation plan for inst "
            << Utils::formatLLVMInst(Inst) << '\n';

        std::string InstName = Inst->getName();
        if (ExpectedStepStreams.count(InstName) == 0) {
          // We don't care about the step streams of this inst.
          continue;
        }

        if (ActualPlan.Plan == PlanT::STEP) {
          std::unordered_set<std::string> ActualStepStreams;
          for (const auto &StepStream : ActualPlan.getStepStreams()) {
            ActualStepStreams.insert(StepStream->getInst()->getName());
          }
          this->testTwoSets<std::string>(ExpectedStepStreams.at(InstName),
                                         ActualStepStreams);
        }
      }
    }
  }
};
} // namespace

TEST_F(StreamTransformPassTransformPlanTestFixture, MultipleSteps) {
  this->setUpEnvironment(GetTestInputSource("/stream/inputs/Unrolled.c"));

  this->Pass.runOnModule(*(this->Module));

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  // Simply get the Loop and stream we want to test.
  llvm::DominatorTree DT(*Func);
  llvm::LoopInfo LI;
  LI.analyze(DT);

  auto Loops = LI.getLoopsInPreorder();
  ASSERT_EQ(1, Loops.size()) << "Illegal number of loops in foo.";

  auto Loop = Loops[0];
  ASSERT_EQ("bb5", Loop->getHeader()->getName());

  ExpectedPlanMapT ExpectedTransformPlanTypes = {
      {"bb5",
       {
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
       }},
  };

  ExpectedStepStreamMapT ExpectedStepStreams = {
      {"tmp13", {"tmp6"}},
      {"tmp11", {"tmp7", "tmp8"}},
  };

  this->testTransformPlan(Loop, ExpectedTransformPlanTypes,
                          ExpectedStepStreams);
}

TEST_F(StreamTransformPassTransformPlanTestFixture, BasicListTransverse) {
  this->setUpEnvironment(
      GetTestInputSource("/stream/inputs/BasicListTransverse.c"));

  this->Pass.runOnModule(*(this->Module));

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  // Simply get the Loop and stream we want to test.
  llvm::DominatorTree DT(*Func);
  llvm::LoopInfo LI;
  LI.analyze(DT);

  auto Loops = LI.getLoopsInPreorder();
  ASSERT_EQ(2, Loops.size()) << "Illegal number of loops in foo.";

  auto Loop = Loops[0];
  ASSERT_EQ("bb5", Loop->getHeader()->getName());

  ExpectedPlanMapT ExpectedTransformPlanTypes = {
      {"bb13",
       {
           PlanT::DELETE,  // phi
           PlanT::DELETE,  // gep
           PlanT::DELETE,  // load
           PlanT::STORE,   // store
           PlanT::DELETE,  // gep
           PlanT::STEP,    // load
           PlanT::NOTHING, // icmp
           PlanT::NOTHING, // br
       }},
  };

  ExpectedStepStreamMapT ExpectedStepStreams;

  this->testTransformPlan(Loop, ExpectedTransformPlanTypes,
                          ExpectedStepStreams);
}
