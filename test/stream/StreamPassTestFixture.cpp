#include "StreamPassTestFixture.h"

#include "llvm/ADT/STLExtras.h"

void StreamTransformPassTestFixture::SetUp() {
  this->Input = nullptr;
  this->Module = nullptr;
}

void StreamTransformPassTestFixture::TearDown() {}

void StreamTransformPassTestFixture::setUpEnvironment(
    const std::string &InputSourceFile) {

  this->Input = llvm::make_unique<TestInput>(InputSourceFile, "stream");
  this->Module = makeLLVMModule(this->Context, this->Input->getBitCodeFile());
  assert(this->Module != nullptr && "Failed to initialize the module.");

  /**
   * We need to manually set all the options.
   */
  this->Input->setUpLLVMOptions("stream");

  this->OutputExtraFolder =
      this->Input->getAndCreateOutputDataGraphExtraFolder("stream");

  //   this->Pass.doInitialization(*this->Module);
}