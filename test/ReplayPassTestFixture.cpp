
#include "ReplayPassTestFixture.h"

void ReplayPassTestFixture::SetUp() {
  this->Input = nullptr;
  this->Module = nullptr;
}

void ReplayPassTestFixture::TearDown() {}

void ReplayPassTestFixture::setUpEnvironment(
    const std::string &InputSourceFile) {

  this->Input = std::make_unique<TestInput>(InputSourceFile);
  this->Module = makeLLVMModule(this->Context, this->Input->getBitCodeFile());

  /**
   * We need to manually set all the options.
   */
  this->Input->setUpLLVMOptions("replay");

  // this->ReplayPass.doInitialization(*this->Module);
}