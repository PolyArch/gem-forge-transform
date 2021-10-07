#include "ReplayPassTestFixture.h"

TEST_F(ReplayPassTestFixture, HelloWorld) {

  this->setUpEnvironment(GetTestInputSource("/stream/inputs/Unrolled.c"));

  EXPECT_TRUE(true) << "Hello world test case for " << GetTestInputSource("/stream/inputs/Unrolled.c");
}

// TEST_F(ReplayPassTestFixture, HelloWorld2) {
//   EXPECT_TRUE(true) << "Hello world test case!";
// }