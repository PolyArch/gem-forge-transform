#ifndef LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H
#define LLVM_TDG_GEM5_PROTOBUF_SERIALIZER_H

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/message.h>

#include <fstream>
#include <string>

class ProtobufSerializer {
public:
  ProtobufSerializer(const std::string &FileName,
                     std::ios_base::openmode OpenMode);
  virtual ~ProtobufSerializer();

  virtual void serialize(const google::protobuf::Message &Message) = 0;

protected:
  std::ofstream OutFileStream;
  google::protobuf::io::ZeroCopyOutputStream *OutZeroCopyStream;
};

class Gem5ProtobufSerializer : public ProtobufSerializer {
public:
  Gem5ProtobufSerializer(const std::string &FileName);

  void serialize(const google::protobuf::Message &Message) override;
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
};

#endif