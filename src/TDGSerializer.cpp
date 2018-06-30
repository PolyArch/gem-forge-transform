#include "TDGSerializer.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

TDGSerializer::TDGSerializer(const std::string &FileName)
    : OutFileStream(FileName, std::ios::out | std::ios::binary),
      SerializedInsts(0) {
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

  delete this->OutZeroCopyStream;
  this->OutZeroCopyStream = nullptr;
  this->OutFileStream.close();
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
  assert(this->TDG.instructions_size() > 0 &&
         "Nothing to write for TDG serializer.");
  this->SerializedInsts += this->TDG.instructions_size();
  // Create the coded stream every time due to its size limit.
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteVarint32(this->TDG.ByteSize());
  this->TDG.SerializeWithCachedSizes(&CodedStream);
  this->TDG.clear_instructions();
}
