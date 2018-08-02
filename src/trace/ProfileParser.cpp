#include "ProfileParser.h"

#include <fstream>

static llvm::cl::opt<std::string>
    ProfileFileName("tdg-profile-file",
                    llvm::cl::desc("LLVM TDG Profile file."));

ProfileParser::ProfileParser() {
  std::string FileName("llvm.profile");
  if (ProfileFileName.getNumOccurrences() > 0) {
    FileName = ProfileFileName.getValue();
  }
  std::ifstream File(FileName);
  assert(File.is_open() && "Failed openning profile file.");
  this->Profile.ParseFromIstream(&File);
  File.close();
}

uint64_t ProfileParser::countBasicBlock(llvm::BasicBlock *BB) const {
  auto BBName = BB->getName().str();
  auto FuncName = BB->getParent()->getName().str();
  const auto &FuncMap = this->Profile.funcs();
  if (FuncMap.find(FuncName) == FuncMap.end()) {
    return 0;
  }
  const auto &BBMap = FuncMap.at(FuncName).bbs();
  if (BBMap.find(BBName) == BBMap.end()) {
    return 0;
  }
  return BBMap.at(BBName);
}