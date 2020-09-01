#ifndef LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H
#define LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H

#include "ProtobufSerializer.h"

class Gem5ProtobufSerializer : public GzipMultipleProtobufSerializer {
public:
  Gem5ProtobufSerializer(const std::string &FileName);
};

class TextProtobufSerializer : public ProtobufSerializer {
public:
  TextProtobufSerializer(const std::string &FileName)
      : ProtobufSerializer(FileName, std::ios::out) {}

  void serialize(const google::protobuf::Message &Message) override;
};

class Gem5ProtobufReader {
public:
  Gem5ProtobufReader(const std::string &FileName);
  ~Gem5ProtobufReader();

  bool read(google::protobuf::Message &Message);

private:
  std::ifstream InFileStream;
  google::protobuf::io::ZeroCopyInputStream *InZeroCopyStream;
  google::protobuf::io::GzipInputStream *GzipStream;
};

#endif