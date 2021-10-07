#include "ProfileParser.h"

#include <fstream>

llvm::cl::opt<std::string>
    ProfileFolder("gem-forge-profile-folder",
                  llvm::cl::desc("GemForge profile folder."));

ProfileParser::ProfileParser() {
  assert(ProfileFolder.getNumOccurrences() > 0 &&
         "Please specify GemForgeProfileFolder.");
  std::string FileName = ProfileFolder.getValue() + "/0.profile";
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