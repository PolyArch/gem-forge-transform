#ifndef LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H
#define LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>

#include <fstream>
#include <string>

class Gem5ProtobufSerializer {
public:
  Gem5ProtobufSerializer(const std::string &FileName);
  ~Gem5ProtobufSerializer();

  void serialize(const google::protobuf::Message &Message);

private:
  std::ofstream OutFileStream;
  google::protobuf::io::ZeroCopyOutputStream *OutZeroCopyStream;

  /**
   * Copied from gem5/src/proto/protoio.hh.
   * Write this magic number first so that it can be readed by
   * gem5's protoio.
   */
  /// Use the ASCII characters gem5 as our magic number
  static const uint32_t Gem5MagicNumber = 0x356d6567;
};

#endif