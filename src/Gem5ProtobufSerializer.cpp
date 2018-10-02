#include "Gem5ProtobufSerializer.h"

// #include "llvm/Support/raw_ostream.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace {
/**
 * Copied from gem5/src/proto/protoio.hh.
 * Write this magic number first so that it can be readed by
 * gem5's protoio.
 */
static const uint32_t Gem5MagicNumber = 0x356d6567;
} // namespace

Gem5ProtobufSerializer::Gem5ProtobufSerializer(const std::string &FileName)
    : OutFileStream(FileName, std::ios::out | std::ios::binary) {

  // llvm::errs() << "Try to open gem5 protobuf serializer file " << FileName
  //              << '\n';

  assert(this->OutFileStream.is_open() &&
         "Failed to open output gem5 protobuf serialize file.");
  // Create the zero copy stream.
  this->OutZeroCopyStream =
      new google::protobuf::io::OstreamOutputStream(&this->OutFileStream);

  // We have to write the magic number so that it can be
  // recognized by gem5.

  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteLittleEndian32(Gem5MagicNumber);
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

Gem5ProtobufReader::Gem5ProtobufReader(const std::string &FileName)
    : InFileStream(FileName, std::ios::in | std::ios::binary) {

  assert(this->InFileStream.is_open() &&
         "Failed to open input gem5 protobuf serialize file.");
  // Create the zero copy stream.
  this->InZeroCopyStream =
      new google::protobuf::io::IstreamInputStream(&this->InFileStream);

  uint32_t MagicNum;
  google::protobuf::io::CodedInputStream CodedStream(this->InZeroCopyStream);
  assert(CodedStream.ReadLittleEndian32(&MagicNum) &&
         "Failed to read in the magic number.");
  assert(MagicNum == Gem5MagicNumber && "Mismatch gem5 magic number.");
}

Gem5ProtobufReader::~Gem5ProtobufReader() {
  delete this->InZeroCopyStream;
  this->InZeroCopyStream = nullptr;
  this->InFileStream.close();
}

bool Gem5ProtobufReader::read(google::protobuf::Message &Message) {
  uint32_t Size;
  google::protobuf::io::CodedInputStream CodedStream(this->InZeroCopyStream);
  if (CodedStream.ReadVarint32(&Size)) {
    google::protobuf::io::CodedInputStream::Limit Limit =
        CodedStream.PushLimit(Size);
    if (Message.ParseFromCodedStream(&CodedStream)) {
      CodedStream.PopLimit(Limit);
      // All went well, the message is parsed and the limit is
      // popped again
      return true;
    } else {
      assert(false && "Failed to read message from gem5 protobuf stream.");
    }
  }

  return false;
}