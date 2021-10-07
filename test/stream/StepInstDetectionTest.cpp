#include "StreamPassTestFixture.h"

class StreamTransformPassStepInstDetectionTestFixture
    : public StreamTransformPassTestFixture {
protected:
  using ExpectedStepInstNameMapT =
      std::unordered_map<std::string, std::unordered_set<std::string>>;

  void
  testStepInstDetection(llvm::Loop *Loop, llvm::ScalarEvolution *SE,
                        const llvm::PostDominatorTree *PDT,
                        const ExpectedStepInstNameMapT &ExpectedStepInsts) {
    llvm::DataLayout DataLayout(this->Module.get());

    // Create the IVStreams for all the phi nodes.
    auto PHIRange = Loop->getHeader()->phis();
    for (auto PHIIter = PHIRange.begin(), PHIEnd = PHIRange.end();
         PHIIter != PHIEnd; ++PHIIter) {
      auto PHIInst = &*PHIIter;
      auto ExpectedStepInstsIter =
          ExpectedStepInsts.find(PHIInst->getName().str());
      if (ExpectedStepInstsIter == ExpectedStepInsts.end()) {
        // We do not expect this PHIInst to be an iv stream.
        continue;
      }
      StaticIndVarStream StaticIVStream(PHIInst, Loop, Loop, SE, PDT,
                                        &DataLayout);
      DynIndVarStream IVStream(this->OutputExtraFolder, "", &StaticIVStream,
                               &DataLayout);
      const auto &ExpectedStepInstNames = ExpectedStepInstsIter->second;
      std::unordered_set<std::string> ActualStepInstNames;
      for (auto StepInst : IVStream.getStepInsts()) {
        EXPECT_EQ(
            1, ExpectedStepInstsIter->second.count(StepInst->getName().str()))
            << "Extra step instruction " << Utils::formatLLVMInst(StepInst)
            << '\n';
        ActualStepInstNames.insert(StepInst->getName().str());
      }
      if (ActualStepInstNames.size() != ExpectedStepInstNames.size()) {
        for (const auto &ExpectedStepInstName : ExpectedStepInstNames) {
          EXPECT_EQ(1, ActualStepInstNames.count(ExpectedStepInstName))
              << "Missting step instruction " << ExpectedStepInstName << '\n';
        }
      }
    }
  }
};

TEST_F(StreamTransformPassStepInstDetectionTestFixture,
       StepInstDetectionTestSparseVecDotVec) {
  this->setUpEnvironment(
      GetTestInputSource("/stream/inputs/SparseVecDotVec.c"));

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  // Simply get the Loop and stream we want to test.
  CachedLoopInfo CachedLI(this->Module.get());
  CachedPostDominanceFrontier CachedPDF;
  auto LI = CachedLI.getLoopInfo(Func);
  auto SE = CachedLI.getScalarEvolution(Func);
  auto PDT = CachedPDF.getPostDominatorTree(Func);

  auto BBIter = Func->begin();
  ++BBIter;
  auto Loop = LI->getLoopFor(&*BBIter);
  ASSERT_NE(nullptr, Loop) << "Failed to find the loop.";
  ASSERT_EQ("bb4", Loop->getHeader()->getName());

  ExpectedStepInstNameMapT ExpectedStepInstNameMap = {
      {"tmp", {"tmp21", "tmp26"}},
      {"tmp5", {"tmp22", "tmp28"}},
  };

  this->testStepInstDetection(Loop, SE, PDT, ExpectedStepInstNameMap);
}

TEST_F(StreamTransformPassStepInstDetectionTestFixture,
       StepInstDetectionTestUnrolled) {
  this->setUpEnvironment(GetTestInputSource("/stream/inputs/Unrolled.c"));

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  // Simply get the Loop and stream we want to test.
  CachedLoopInfo CachedLI(this->Module.get());
  CachedPostDominanceFrontier CachedPDF;
  auto LI = CachedLI.getLoopInfo(Func);
  auto SE = CachedLI.getScalarEvolution(Func);
  auto PDT = CachedPDF.getPostDominatorTree(Func);
  auto Loop = *LI->begin();
  ASSERT_NE(nullptr, Loop) << "Failed to find the loop.";
  ASSERT_EQ("bb5", Loop->getHeader()->getName());

  ExpectedStepInstNameMapT ExpectedStepInstNameMap = {
      {"tmp6", {"tmp13"}},
      {"tmp7", {"tmp11"}},
      {"tmp8", {"tmp11"}},
  };

  this->testStepInstDetection(Loop, SE, PDT, ExpectedStepInstNameMap);
}