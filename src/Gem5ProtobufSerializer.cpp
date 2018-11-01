#include "Gem5ProtobufSerializer.h"

#include "llvm/Support/raw_ostream.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

namespace {
/**
 * Copied from gem5/src/proto/protoio.hh.
 * Write this magic number first so that it can be readed by
 * gem5's protoio.
 */
static const uint32_t Gem5MagicNumber = 0x356d6567;
} // namespace

ProtobufSerializer::ProtobufSerializer(const std::string &FileName,
                                       std::ios_base::openmode OpenMode) {

  llvm::errs() << "Try to open protobuf serializer file " << FileName << '\n';
  this->OutFileStream.open(FileName, OpenMode);

  assert(this->OutFileStream.is_open() &&
         "Failed to open output protobuf serialize file.");
  // Create the zero copy stream.
  this->OutZeroCopyStream =
      new google::protobuf::io::OstreamOutputStream(&this->OutFileStream);
}

ProtobufSerializer::~ProtobufSerializer() {
  delete this->OutZeroCopyStream;
  this->OutZeroCopyStream = nullptr;
  this->OutFileStream.close();
}

Gem5ProtobufSerializer::Gem5ProtobufSerializer(const std::string &FileName)
    : ProtobufSerializer(FileName, std::ios::out | std::ios::binary) {
  // Write the magic number so that gem5 can read this.
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteLittleEndian32(Gem5MagicNumber);
}

void Gem5ProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  google::protobuf::io::CodedOutputStream CodedStream(this->OutZeroCopyStream);
  CodedStream.WriteVarint32(Message.ByteSize());
  Message.SerializeWithCachedSizes(&CodedStream);
}

void TextProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  google::protobuf::TextFormat::Print(Message, this->OutZeroCopyStream);
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