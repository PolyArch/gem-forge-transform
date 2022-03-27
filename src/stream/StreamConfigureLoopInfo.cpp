#include "StreamConfigureLoopInfo.h"

StreamConfigureLoopInfo::StreamConfigureLoopInfo(
    const std::string &_Folder, const std::string &_RelativeFolder,
    const llvm::Loop *_Loop, std::vector<StaticStream *> _SortedStreams)
    : TotalConfiguredStreams(-1), TotalConfiguredCoalescedStreams(-1),
      TotalSubLoopStreams(-1), TotalSubLoopCoalescedStreams(-1),
      TotalAliveStreams(-1), TotalAliveCoalescedStreams(-1),
      Path(_Folder + "/" + LoopUtils::getLoopId(_Loop) + ".info"),
      JsonPath(_Folder + "/" + LoopUtils::getLoopId(_Loop) + ".json"),
      RelativePath(_RelativeFolder + "/" + LoopUtils::getLoopId(_Loop) +
                   ".info"),
      Loop(_Loop), SortedStreams(std::move(_SortedStreams)) {}

void StreamConfigureLoopInfo::dump(llvm::DataLayout *DataLayout) const {
  LLVM::TDG::StreamRegion ProtobufStreamRegion;
  ProtobufStreamRegion.set_region(LoopUtils::getLoopId(this->Loop));
  ProtobufStreamRegion.set_relative_path(this->getRelativePath());
  ProtobufStreamRegion.set_total_alive_streams(this->TotalAliveStreams);
  ProtobufStreamRegion.set_total_alive_coalesced_streams(
      this->TotalAliveCoalescedStreams);
  ProtobufStreamRegion.set_loop_eliminated(this->LoopEliminated);
  for (auto &S : this->SortedStreams) {
    auto Info = ProtobufStreamRegion.add_streams();
    S->fillProtobufStreamInfo(Info);
  }
  for (auto &S : this->SortedCoalescedStreams) {
    ProtobufStreamRegion.add_coalesced_stream_ids(S->StreamId);
  }
  // Remember nest stream region information.
  for (auto NestConfigureInfo : this->NestConfigureInfos) {
    ProtobufStreamRegion.add_nest_region_relative_paths(
        NestConfigureInfo->getRelativePath());
  }
  if (this->NestConfigureFuncInfo) {
    *ProtobufStreamRegion.mutable_nest_config_func() =
        *this->NestConfigureFuncInfo;
    ProtobufStreamRegion.set_is_nest(true);
    if (this->NestPredFuncInfo) {
      *ProtobufStreamRegion.mutable_nest_pred_func() = *this->NestPredFuncInfo;
      ProtobufStreamRegion.set_nest_pred_ret(this->NestPredRet);
    }
  }
  if (this->LoopBoundFuncInfo) {
    *ProtobufStreamRegion.mutable_loop_bound_func() = *this->LoopBoundFuncInfo;
    ProtobufStreamRegion.set_is_loop_bound(true);
    ProtobufStreamRegion.set_loop_bound_ret(this->LoopBoundRet);
  }
  Gem5ProtobufSerializer Serializer(this->Path);
  Serializer.serialize(ProtobufStreamRegion);
  Utils::dumpProtobufMessageToJson(ProtobufStreamRegion, this->JsonPath);
}

void StreamConfigureLoopInfo::addNestConfigureInfo(
    StreamConfigureLoopInfo *NestConfigureInfo,
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> NestConfigureFuncInfo,
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> PredFuncInfo,
    bool PredicateRet) {
  assert(!NestConfigureInfo->NestConfigureFuncInfo && "Region already nested.");
  NestConfigureInfo->NestConfigureFuncInfo = std::move(NestConfigureFuncInfo);
  NestConfigureInfo->NestPredFuncInfo = std::move(PredFuncInfo);
  NestConfigureInfo->NestPredRet = PredicateRet;
  this->NestConfigureInfos.push_back(NestConfigureInfo);
}

void StreamConfigureLoopInfo::addLoopBoundDG(
    std::unique_ptr<::LLVM::TDG::ExecFuncInfo> LoopBoundFuncInfo,
    bool LoopBoundRet) {
  assert(!this->LoopBoundFuncInfo && "Multile LoopBoundDG.");
  this->LoopBoundFuncInfo = std::move(LoopBoundFuncInfo);
  this->LoopBoundRet = LoopBoundRet;
}

void StreamConfigureLoopInfo::setLoopEliminated(bool LoopEliminated) {
  this->LoopEliminated = LoopEliminated;
}