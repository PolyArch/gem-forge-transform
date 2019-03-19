#include "Gem5ProtobufSerializer.h"

#include "llvm/Support/raw_ostream.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

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
  this->GzipStream =
      new google::protobuf::io::GzipOutputStream(this->OutZeroCopyStream);
  // Write the magic number so that gem5 can read this.
  google::protobuf::io::CodedOutputStream CodedStream(this->GzipStream);
  CodedStream.WriteLittleEndian32(Gem5MagicNumber);
}

Gem5ProtobufSerializer::~Gem5ProtobufSerializer() {
  delete this->GzipStream;
  this->GzipStream = nullptr;
}

void Gem5ProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  google::protobuf::io::CodedOutputStream CodedStream(this->GzipStream);
  CodedStream.WriteVarint32(Message.ByteSize());
  Message.SerializeWithCachedSizes(&CodedStream);
}

void TextProtobufSerializer::serialize(
    const google::protobuf::Message &Message) {
  std::string JsonString;
  // For the json file, we do not log history.
  google::protobuf::util::MessageToJsonString(Message, &JsonString);
  // Write directly to the file stream.
  this->OutFileStream << "=============== " << Message.ByteSize() << '\n';
  this->OutFileStream << JsonString << '\n';
}

Gem5ProtobufReader::Gem5ProtobufReader(const std::string &FileName)
    : InFileStream(FileName, std::ios::in | std::ios::binary) {
  assert(this->InFileStream.is_open() &&
         "Failed to open input gem5 protobuf serialize file.");
  // Create the zero copy stream.
  this->InZeroCopyStream =
      new google::protobuf::io::IstreamInputStream(&this->InFileStream);
  this->GzipStream =
      new google::protobuf::io::GzipInputStream(this->InZeroCopyStream);

  uint32_t MagicNum;
  google::protobuf::io::CodedInputStream CodedStream(this->GzipStream);
  assert(CodedStream.ReadLittleEndian32(&MagicNum) &&
         "Failed to read in the magic number.");
  assert(MagicNum == Gem5MagicNumber && "Mismatch gem5 magic number.");
}

Gem5ProtobufReader::~Gem5ProtobufReader() {
  delete this->GzipStream;
  this->GzipStream = nullptr;
  delete this->InZeroCopyStream;
  this->InZeroCopyStream = nullptr;
  this->InFileStream.close();
}

bool Gem5ProtobufReader::read(google::protobuf::Message &Message) {
  uint32_t Size;
  google::protobuf::io::CodedInputStream CodedStream(this->GzipStream);
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