#include "ProfileLogger.h"
#include "ProfileMessage.pb.h"

#include <google/protobuf/text_format.h>

#include <fstream>
#include <iomanip>

ProfileLogger::ProfileLogger() : BBCount(0) {}

void ProfileLogger::addBasicBlock(const std::string &Func,
                                  const std::string &BB) {
  // operator[] will implicitly create the BBMap;
  auto &BBMap = Profile[Func];
  if (BBMap.find(BB) == BBMap.end()) {
    BBMap.emplace(BB, 1);
  } else {
    BBMap.at(BB)++;
  }

  // Update the function count.
  this->FuncProfile.emplace(Func, 0).first->second++;

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

  // For convenience, we will sort all the functions by their dynamic count
  // and dump to a txt file.
  {
    std::vector<std::string> SortedFunctions;
    SortedFunctions.reserve(this->FuncProfile.size());
    for (const auto &FuncCountPair : this->FuncProfile) {
      SortedFunctions.push_back(FuncCountPair.first);
    }
    std::sort(SortedFunctions.begin(), SortedFunctions.end(),
              [this](const std::string &FA, const std::string &FB) -> bool {
                return this->FuncProfile.at(FA) > this->FuncProfile.at(FB);
              });

    std::ofstream File(FileName + ".txt");
    assert(File.is_open() && "Failed openning profile txt file.");
    const double Threshold = 0.99999;
    double Accumulated = 0.0;
    for (const auto &Func : SortedFunctions) {
      double Percentage = static_cast<double>(this->FuncProfile.at(Func)) /
                          static_cast<double>(this->BBCount);
      File << std::setw(7) << std::fixed << std::setprecision(4) << Accumulated
           << std::setw(7) << std::fixed << std::setprecision(4) << Percentage
           << ' ' << Func << '\n';
      Accumulated += Percentage;
      if (Accumulated > Threshold) {
        break;
      }
    }
    File.close();
  }

  // std::string OutString;
  // google::protobuf::TextFormat::PrintToString(ProtobufProfile, &OutString);
  // std::cout << OutString << std::endl;

  std::cout << "Profiled Basic Block #" << BBCount << std::endl;
}