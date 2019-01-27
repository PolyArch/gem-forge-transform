#ifndef LLVM_TDG_LOOP_UNROLLER_TEST_FIXTURE_H
#define LLVM_TDG_LOOP_UNROLLER_TEST_FIXTURE_H

#include "TestSetup.h"

#include "LoopUnroller.h"

class LoopUnrollerTestFixture : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  virtual void setUpEnvironment(const std::string &InputSourceFile);

  llvm::LLVMContext Context;
  std::unique_ptr<llvm::Module> Module;

  std::unique_ptr<TestInput> Input;
  std::string OutputExtraFolder;

  std::unique_ptr<DataGraph> DG;
  std::unique_ptr<TDGSerializer> Serializer;
};

void LoopUnrollerTestFixture::SetUp() {
  this->Input = nullptr;
  this->Module = nullptr;
  this->DG = nullptr;
  this->Serializer = nullptr;
}

void LoopUnrollerTestFixture::TearDown() {
  this->Input = nullptr;
  this->Module = nullptr;
  this->DG = nullptr;
  this->Serializer = nullptr;
}

void LoopUnrollerTestFixture::setUpEnvironment(
    const std::string &InputSourceFile) {
  this->Input = std::make_unique<TestInput>(InputSourceFile, "");
  this->Module = makeLLVMModule(this->Context, this->Input->getBitCodeFile());
  assert(this->Module != nullptr && "Failed to initialize the module.");

  // Set up all the llvm options.
  this->Input->setUpLLVMOptions("");

  // Create the DG.
  this->DG = std::make_unique<DataGraph>(
      this->Module.get(), DataGraph::DataGraphDetailLv::STANDALONE);
  this->Serializer = std::make_unique<TDGSerializer>(
      this->Input->getOutputDataGraphFile(), true);
}

TEST_F(LoopUnrollerTestFixture, UnrollDependenceTestEmptyLoopBody) {
  this->setUpEnvironment(
      GetTestInputSource("/loop_unroller/inputs/EmptyLoopBody.c"));

  // Get the function.
  auto Func = this->Module->getFunction("foo");
  ASSERT_NE(nullptr, Func) << "Failed to get the foo testing function.";

  // Get the loop.
  llvm::DominatorTree DT(*Func);
  llvm::LoopInfo LI;
  LI.analyze(DT);

  auto BBIter = Func->begin();
  auto Loop = LI.getLoopsInPreorder()[0];
  ASSERT_NE(nullptr, Loop) << "Failed to find the loop.";
  ASSERT_EQ("bb2", Loop->getHeader()->getName());

  // Get TLI.
  llvm::TargetLibraryInfoImpl TLIImpl(
      llvm::Triple(this->Module->getTargetTriple()));
  llvm::TargetLibraryInfo TLI(TLIImpl);

  // Get assumption cache.
  llvm::AssumptionCache AC(*Func);

  // Get the SCEV;
  llvm::ScalarEvolution SE(*Func, TLI, AC, DT, LI);

  // Create the unroller.
  LoopUnroller LU(Loop, &SE);

  this->Serializer->serializeStaticInfo(LLVM::TDG::StaticInformation());

  // Store the loop iteration start points.
  std::vector<DataGraph::DynamicInstIter> LoopIterationPoints;

  llvm::Loop *NewLoop = nullptr;
  llvm::Loop *OldLoop = nullptr;

  while (true) {
    auto NewDynamicInst = this->DG->loadOneDynamicInst();

    llvm::Instruction *NewStaticInst = nullptr;
    OldLoop = NewLoop;
    NewLoop = nullptr;
    bool IsAtHeaderOfCandidate = false;

    if (NewDynamicInst != this->DG->DynamicInstructionList.end()) {
      // This is a new instruction.
      NewStaticInst = (*NewDynamicInst)->getStaticInstruction();
      assert(NewStaticInst != nullptr && "Invalid static llvm instruction.");

      if (Loop->contains(NewStaticInst)) {
        NewLoop = Loop;
        IsAtHeaderOfCandidate =
            LoopUtils::isStaticInstLoopHead(Loop, NewStaticInst);
        if (IsAtHeaderOfCandidate) {
          LoopIterationPoints.push_back(NewDynamicInst);
        }
      } else {
        // Check if we are just out of the loop.
        if (OldLoop == Loop && NewLoop == nullptr) {
          LoopIterationPoints.push_back(NewDynamicInst);
        }
      }
    } else {
      // Assert we have 12 iterations.
      const int TRUE_ITERS = 12;
      ASSERT_EQ(TRUE_ITERS + 1, LoopIterationPoints.size())
          << "Unmatched number of iterations.";

      // Unroll the loops.
      const int UNROLL_WIDTH = 4;
      for (int Iter = 0; Iter < TRUE_ITERS; Iter += UNROLL_WIDTH) {
        LU.unroll(this->DG.get(), LoopIterationPoints[Iter],
                  LoopIterationPoints[Iter + UNROLL_WIDTH]);
      }

      // End of the trace.
      while (!this->DG->DynamicInstructionList.empty()) {
        this->Serializer->serialize(this->DG->DynamicInstructionList.front(),
                                    this->DG.get());
        this->DG->commitOneDynamicInst();
      }
      break;
    }
  }
}

#endif