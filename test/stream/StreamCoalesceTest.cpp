#include "StreamPassTestFixture.h"

TEST_F(StreamTransformPassTestFixture, Coalesce) {
  this->setUpEnvironment(GetTestInputSource("/stream/inputs/Coalesce.c"));

  this->Pass.runOnModule(*(this->Module));

  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  llvm::Instruction *InstA = nullptr;
  llvm::Instruction *InstB = nullptr;

  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    if (BBIter->getName() != "bb3") {
      continue;
    }
    for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
         InstIter != InstEnd; ++InstIter) {
      if (InstIter->getName() == "tmp6") {
        InstA = &*InstIter;
      } else if (InstIter->getName() == "tmp9") {
        InstB = &*InstIter;
      }
    }
  }

  ASSERT_TRUE(InstA != nullptr) << "Failed to find InstA.";
  ASSERT_TRUE(InstB != nullptr) << "Failed to find InstB.";
}