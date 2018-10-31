#ifndef LLVM_TDG_REPLAY_PASS_TEST_FIXTURE_H
#define LLVM_TDG_REPLAY_PASS_TEST_FIXTURE_H

#include "TestSetup.h"

class ReplayPassTestFixture : public testing::Test {
protected:
  virtual void SetUp();
  virtual void TearDown();

  /**
   * Helper function to setup the test environment.
   */
  virtual void setUpEnvironment(const std::string &InputSourceFile);

  llvm::LLVMContext Context;
  std::unique_ptr<llvm::Module> Module;
  ReplayTrace ReplayPass;

  std::unique_ptr<TestInput> Input;
};

#endif