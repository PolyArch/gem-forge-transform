#include "Gem5ProtobufSerializer.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

ProtobufSerializer::ProtobufSerializer(const std::string &FileName,
                                       std::ios_base::openmode OpenMode) {
  this->OutFileStream.open(FileName, OpenMode);

  if (!this->OutFileStream.is_open()) {
    printf("Failed to open output protobuf serialize file %s.\n",
           FileName.c_str());
    assert(false && "Failed to open output protobuf serialize file.");
  }
  // Create the zero copy stream.
  this->OutZeroCopyStream =
      new google::protobuf::io::OstreamOutputStream(&this->OutFileStream);
}

ProtobufSerializer::~ProtobufSerializer() {
  delete this->OutZeroCopyStream;
  this->OutZeroCopyStream = nullptr;
  this->OutFileStream.close();
}

GzipMultipleProtobufSerializer::GzipMultipleProtobufSerializer(
    const std::string &FileName)
    : ProtobufSerializer(FileName, std::ios::out | std::ios::binary) {
  this->GzipStream =
      new google::protobuf::io::GzipOutputStream(this->OutZeroCopyStream);
  this->CodedStream =
      new google::protobuf::io::CodedOutputStream(this->GzipStream);
}

GzipMultipleProtobufSerializer::~GzipMultipleProtobufSerializer() {
  delete this->CodedStream;
  this->CodedStream = nullptr;
  delete this->GzipStream;
  this->GzipStream = nullptr;
}

void GzipMultipleProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  this->CodedStream->WriteVarint32(Message.ByteSize());
  Message.SerializeWithCachedSizes(CodedStream);
}