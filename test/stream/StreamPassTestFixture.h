#ifndef LLVM_TDG_STREAM_PASS_TEST_FIXTURE_H
#define LLVM_TDG_STREAM_PASS_TEST_FIXTURE_H

#include "TestSetup.h"

#include "stream/StreamPass.h"

class StreamTransformPassTestFixture : public testing::Test {
protected:
  virtual void SetUp();
  virtual void TearDown();

  /**
   * Helper function to setup the test environment.
   */
  virtual void setUpEnvironment(const std::string &InputSourceFile);

  template <typename T>
  void testTwoSets(const std::unordered_set<T> &Expected,
                   const std::unordered_set<T> &Actual,
                   std::function<std::string(const T &)> Formatter) {
    EXPECT_EQ(Expected.size(), Actual.size());
    for (const auto &E : Expected) {
      EXPECT_EQ(1, Actual.count(E))
          << "Missing element " << Formatter(E) << '\n';
    }
    for (const auto &E : Actual) {
      EXPECT_EQ(1, Expected.count(E))
          << "Extra element " << Formatter(E) << '\n';
    }
  }

  using PlanT = StreamTransformPlan::PlanT;

  llvm::LLVMContext Context;
  std::unique_ptr<llvm::Module> Module;
  StreamPass Pass;

  std::unique_ptr<TestInput> Input;
  std::string OutputExtraFolder;
};

#endif