#include "Gem5ProtobufSerializer.h"

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

Gem5ProtobufSerializer::Gem5ProtobufSerializer(const std::string &FileName)
    : GzipMultipleProtobufSerializer(FileName) {
  // Write the magic number so that gem5 can read this.
  CodedStream->WriteLittleEndian32(Gem5MagicNumber);
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
    : GzipMultipleProtobufReader(FileName) {

  uint32_t MagicNum;
  assert(this->CodedStream->ReadLittleEndian32(&MagicNum) &&
         "Failed to read in the magic number.");
  assert(MagicNum == Gem5MagicNumber && "Mismatch gem5 magic number.");
}