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
  ProtobufStreamRegion.set_total_alive_streams(this->TotalAliveStreams);
  ProtobufStreamRegion.set_total_alive_coalesced_streams(
      this->TotalAliveCoalescedStreams);
  for (auto &S : this->SortedStreams) {
    auto Info = ProtobufStreamRegion.add_streams();
    S->fillProtobufStreamInfo(Info);
  }
  for (auto &S : this->SortedCoalescedStreams) {
    ProtobufStreamRegion.add_coalesced_stream_ids(S->StreamId);
  }
  Gem5ProtobufSerializer Serializer(this->Path);
  Serializer.serialize(ProtobufStreamRegion);
  Utils::dumpProtobufMessageToJson(ProtobufStreamRegion, this->JsonPath);
}
