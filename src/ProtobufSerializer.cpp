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
  this->CodedStream->WriteVarint32(Message.ByteSizeLong());
  Message.SerializeWithCachedSizes(CodedStream);
}

void GzipMultipleProtobufSerializer::writeVarint64(uint64_t V) {
  this->CodedStream->WriteVarint64(V);
}

GzipMultipleProtobufReader::GzipMultipleProtobufReader(
    const std::string &FileName)
    : InFileStream(FileName, std::ios::in | std::ios::binary) {
  assert(this->InFileStream.is_open() &&
         "Failed to open input gem5 protobuf serialize file.");
  // Create the zero copy stream.
  this->InZeroCopyStream =
      new google::protobuf::io::IstreamInputStream(&this->InFileStream);
  this->GzipStream =
      new google::protobuf::io::GzipInputStream(this->InZeroCopyStream);
  this->CodedStream =
      new google::protobuf::io::CodedInputStream(this->GzipStream);
}

GzipMultipleProtobufReader::~GzipMultipleProtobufReader() {
  delete this->CodedStream;
  this->CodedStream = nullptr;
  delete this->GzipStream;
  this->GzipStream = nullptr;
  delete this->InZeroCopyStream;
  this->InZeroCopyStream = nullptr;
  this->InFileStream.close();
}

bool GzipMultipleProtobufReader::read(google::protobuf::Message &Message) {
  this->resetCodedStream();
  uint32_t Size;
  if (this->CodedStream->ReadVarint32(&Size)) {
    google::protobuf::io::CodedInputStream::Limit Limit =
        this->CodedStream->PushLimit(Size);
    if (Message.ParseFromCodedStream(this->CodedStream)) {
      this->CodedStream->PopLimit(Limit);
      // All went well, the message is parsed and the limit is
      // popped again
      return true;
    } else {
      std::cout << "TotalBytesLimit " << this->CodedStream->CurrentPosition()
                << " " << this->CodedStream->BytesUntilTotalBytesLimit()
                << '\n';
      assert(false && "Failed to read message from gem5 protobuf stream.");
    }
  }

  return false;
}

bool GzipMultipleProtobufReader::readVarint64(uint64_t &V) {
  this->resetCodedStream();
  return this->CodedStream->ReadVarint64(&V);
}

void GzipMultipleProtobufReader::resetCodedStream() {
  auto BytesUntilTotalBytesLimit =
      this->CodedStream->BytesUntilTotalBytesLimit();
  auto CurrentPos = this->CodedStream->CurrentPosition();
  if ((BytesUntilTotalBytesLimit >= 0 &&
       BytesUntilTotalBytesLimit < 1024 * 1024) ||
      (BytesUntilTotalBytesLimit == -1 &&
       (INT_MAX - CurrentPos < 1024 * 1024))) {
    delete this->CodedStream;
    this->CodedStream =
        new google::protobuf::io::CodedInputStream(this->GzipStream);
    std::cout << "TotalBytesLimit " << this->CodedStream->CurrentPosition()
              << '\n';
  }
}