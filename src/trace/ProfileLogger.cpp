#include "ProfileLogger.h"
#include "ProfileMessage.pb.h"

#include <google/protobuf/text_format.h>

#include <fstream>
#include <unordered_map>

namespace {
std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>>
    Profile;
uint64_t BBCount = 0;
} // namespace

void ProfileLogger::addBasicBlock(const std::string &Func,
                                  const std::string &BB) {
  // operator[] will implicitly create the BBMap;
  auto &BBMap = Profile[Func];
  if (BBMap.find(BB) == BBMap.end()) {
    BBMap.emplace(BB, 1);
  } else {
    BBMap.at(BB)++;
  }

  BBCount++;
}

void ProfileLogger::serializeToFile(const std::string &FileName) {

  LLVM::TDG::Profile ProtobufProfile;
  ProtobufProfile.set_name("Whatever");
  auto &ProtobufFuncs = *(ProtobufProfile.mutable_funcs());
  for (const auto &FuncMap : Profile) {
    const auto &FuncName = FuncMap.first;
    auto &ProtobufFunc = ProtobufFuncs[FuncName];
    ProtobufFunc.set_func(FuncName);
    auto &ProtobufBBs = *(ProtobufFunc.mutable_bbs());
    for (const auto &BBMap : FuncMap.second) {
      ProtobufBBs[BBMap.first] = BBMap.second;
    }
  }

  std::ofstream File(FileName);
  assert(File.is_open() && "Failed openning serialization profile file.");
  ProtobufProfile.SerializeToOstream(&File);
  File.close();

  // std::string OutString;
  // google::protobuf::TextFormat::PrintToString(ProtobufProfile, &OutString);
  // std::cout << OutString << std::endl;

  std::cout << "Profiled Basic Block #" << BBCount << std::endl;
}