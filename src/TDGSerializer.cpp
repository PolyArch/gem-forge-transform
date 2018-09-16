#include "TDGSerializer.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "TDGSerializer"

TDGSerializer::TDGSerializer(const std::string &FileName)
    : OutFileStream(FileName, std::ios::out | std::ios::binary),
      SerializedStaticInfomation(false), SerializedInsts(0) {
  assert(this->OutFileStream.is_open() &&
         "Failed to open output serialize file.");
  // Create the zero copy stream.
  this->OutZeroCopyStream =
      new google::protobuf::io::OstreamOutputStream(&this->OutFileStream);

  // We have to write the magic number so that it can be
  // recognized by gem5.

  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteLittleEndian32(this->Gem5MagicNumber);
}

TDGSerializer::~TDGSerializer() {
  // Serialize the remainning instructions.
  if (this->TDG.instructions_size() > 0) {
    this->write();
  }

  llvm::errs() << "Serialized " << this->SerializedInsts << '\n';

  delete this->OutZeroCopyStream;
  this->OutZeroCopyStream = nullptr;
  this->OutFileStream.close();
}

void TDGSerializer::serializeStaticInfo(
    const LLVM::TDG::StaticInformation &StaticInfo) {
  assert(!this->SerializedStaticInfomation &&
         "StaticInfomation already serialized.");
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteVarint32(StaticInfo.ByteSize());
  StaticInfo.SerializeWithCachedSizes(&CodedStream);
  this->SerializedStaticInfomation = true;
}

void TDGSerializer::serialize(DynamicInstruction *DynamicInst, DataGraph *DG) {
  auto ProtobufEntry = this->TDG.add_instructions();
  DynamicInst->serializeToProtobuf(ProtobufEntry, DG);

  // Serialize every some instructions.
  if (this->TDG.instructions_size() == 10000) {
    this->write();
  }
}

void TDGSerializer::write() {
  assert(this->SerializedStaticInfomation &&
         "Should serialize the static information first.");
  assert(this->TDG.instructions_size() > 0 &&
         "Nothing to write for TDG serializer.");
  // Hacky to dump some serialized instructions for debug.
  const uint64_t DUMP_START = 0;
  const uint64_t DUMP_END = 1100;
  if (this->SerializedInsts < DUMP_END ||
      this->SerializedInsts + this->TDG.instructions_size() >= DUMP_START) {
    for (uint64_t DUMPED = 0; DUMPED < this->TDG.instructions_size();
         ++DUMPED) {
      auto Id = DUMPED + this->SerializedInsts;
      if (Id < DUMP_START) {
        continue;
      }
      if (Id >= DUMP_END) {
        break;
      }
      std::string OutString;
      DEBUG(google::protobuf::TextFormat::PrintToString(
          this->TDG.instructions(DUMPED), &OutString));
      DEBUG(llvm::errs() << OutString << '\n');
    }
  }
  // Create the coded stream every time due to its size limit.
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  // CodedStream.WriteVarint32(this->TDG.ByteSize());
  // this->TDG.SerializeWithCachedSizes(&CodedStream);

  // Instead of writing them together, let's write one by one.
  for (size_t I = 0; I < this->TDG.instructions_size(); ++I) {
    auto &TDGInstruction = this->TDG.instructions(I);
    CodedStream.WriteVarint32(TDGInstruction.ByteSize());
    TDGInstruction.SerializeWithCachedSizes(&CodedStream);
  }

  this->SerializedInsts += this->TDG.instructions_size();
  this->TDG.clear_instructions();
}

#undef DEBUG_TYPE