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

};

class Gem5ProtobufReader {
public:
  Gem5ProtobufReader(const std::string &FileName);
  ~Gem5ProtobufReader();

  bool read(google::protobuf::Message &Message);

private:
  std::ifstream InFileStream;
  google::protobuf::io::ZeroCopyInputStream *InZeroCopyStream;
};

#endif