#ifndef LLVM_TDG_TEST_SETUP_H
#define LLVM_TDG_TEST_SETUP_H

#include "Replay.h"

#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"

#include "gtest/gtest.h"

#include <string>

std::unique_ptr<llvm::Module> makeLLVMModule(llvm::LLVMContext &Context,
                                             const std::string &ModuleFileName);

// Macro to get the test source files.
// Build system should define the TestDir.
#ifndef TestDir
#error "Build system should define TestDir"
#endif
#define GetTestInputSource(RelativePath) (TestDir RelativePath)

class TestInput {
public:
  TestInput(const std::string &_TestInputSource, const std::string &_Pass);

  const std::string &getBitCodeFile() const { return this->BitCodeFile; }
  const std::string &getInstUidMapFile() const { return this->InstUidMapFile; }
  const std::string &getProfileFile() const { return this->ProfileFile; }
  const std::string &getTraceFile() const { return this->TraceFile; }
  std::string getOutputDataGraphFile() const {
    return this->OutputDataGraphFile;
  }
  std::string getOutputDataGraphTextFile() const {
    return this->OutputDataGraphTextFile;
  }
  std::string
  getAndCreateOutputDataGraphExtraFolder(const std::string &Pass) const;

  void setUpLLVMOptions(const std::string &Pass) const;

private:
  std::string TestInputSource;
  std::string Pass;
  std::string Prefix;
  std::string BitCodeFile;
  std::string InstUidMapFile;
  std::string ProfileFile;
  std::string TraceFile;
  std::string OutputDataGraphFile;
  std::string OutputDataGraphTextFile;
};

#endif