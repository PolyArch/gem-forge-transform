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

  using PlanT = StreamTransformPlan::PlanT;

  llvm::LLVMContext Context;
  std::unique_ptr<llvm::Module> Module;
  StreamPass Pass;

  std::unique_ptr<TestInput> Input;
};

#endif