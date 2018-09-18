#include "Gem5ProtobufSerializer.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

Gem5ProtobufSerializer::Gem5ProtobufSerializer(const std::string &FileName)
    : OutFileStream(FileName, std::ios::out | std::ios::binary) {
  assert(this->OutFileStream.is_open() &&
         "Failed to open output gem5 protobuf serialize file.");
  // Create the zero copy stream.
  this->OutZeroCopyStream =
      new google::protobuf::io::OstreamOutputStream(&this->OutFileStream);

  // We have to write the magic number so that it can be
  // recognized by gem5.

  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteLittleEndian32(this->Gem5MagicNumber);
}

Gem5ProtobufSerializer::~Gem5ProtobufSerializer() {
  delete this->OutZeroCopyStream;
  this->OutZeroCopyStream = nullptr;
  this->OutFileStream.close();
}

void Gem5ProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteVarint32(Message.ByteSize());
  Message.SerializeWithCachedSizes(&CodedStream);
}