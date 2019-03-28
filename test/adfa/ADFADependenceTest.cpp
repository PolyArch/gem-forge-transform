#ifndef LLVM_TDG_LOOP_UNROLLER_TEST_FIXTURE_H
#define LLVM_TDG_LOOP_UNROLLER_TEST_FIXTURE_H

#include "TestSetup.h"

#include "adfa/AbstractDataFlowAcceleratorPass.h"

class ADFADependenceTestFixture : public testing::Test {
protected:
  virtual void SetUp();
  virtual void TearDown();

  virtual void setUpEnvironment(const std::string &InputSourceFile);

  llvm::LLVMContext Context;
  std::unique_ptr<llvm::Module> Module;

  std::unique_ptr<TestInput> Input;
  std::string OutputExtraFolder;

  AbstractDataFlowAcceleratorPass ADFAPass;
};

void ADFADependenceTestFixture::SetUp() {
  this->Input = nullptr;
  this->Module = nullptr;
}

void ADFADependenceTestFixture::TearDown() {
  this->Input = nullptr;
  this->Module = nullptr;
}

void ADFADependenceTestFixture::setUpEnvironment(
    const std::string &InputSourceFile) {
  this->Input = std::make_unique<TestInput>(InputSourceFile, "adfa");
  this->Module = makeLLVMModule(this->Context, this->Input->getBitCodeFile());
  assert(this->Module != nullptr && "Failed to initialize the module.");

  // Set up all the llvm options.
  this->Input->setUpLLVMOptions("adfa");

  this->OutputExtraFolder =
      this->Input->getAndCreateOutputDataGraphExtraFolder("adfa");
}

TEST_F(ADFADependenceTestFixture, UnrollDependenceTestEmptyLoopBody) {
  this->setUpEnvironment(GetTestInputSource("/adfa/inputs/EmptyLoopBody.c"));

  // Run the pass.
  this->ADFAPass.runOnModule(*(this->Module));

  // Get the function.
  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";
}

TEST_F(ADFADependenceTestFixture, UnrollDependenceTestReductionVar) {
  this->setUpEnvironment(GetTestInputSource("/adfa/inputs/ReductionVar.c"));

  // Run the pass.
  this->ADFAPass.runOnModule(*(this->Module));

  // Get the function.
  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";
}

TEST_F(ADFADependenceTestFixture, UnrollDependenceTestNestedLoop) {
  this->setUpEnvironment(GetTestInputSource("/adfa/inputs/NestedLoop.c"));

  // Run the pass.
  this->ADFAPass.runOnModule(*(this->Module));

  // Get the function.
  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";
}

#endif