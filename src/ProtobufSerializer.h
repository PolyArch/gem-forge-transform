#ifndef LLVM_TDG_PROTOBUF_SERIALIZER_H
#define LLVM_TDG_PROTOBUF_SERIALIZER_H

#include <google/protobuf/io/gzip_stream.h>
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
  google::protobuf::io::ZeroCopyOutputStream *OutZeroCopyStream = nullptr;
};

class GzipMultipleProtobufSerializer : public ProtobufSerializer {
public:
  GzipMultipleProtobufSerializer(const std::string &FileName);
  ~GzipMultipleProtobufSerializer() override;

  void serialize(const google::protobuf::Message &Message) override;

protected:
  google::protobuf::io::GzipOutputStream *GzipStream = nullptr;
  google::protobuf::io::CodedOutputStream *CodedStream = nullptr;
};

class GzipMultipleProtobufReader {
public:
  GzipMultipleProtobufReader(const std::string &FileName);
  virtual ~GzipMultipleProtobufReader();

  bool read(google::protobuf::Message &Message);

protected:
  std::ifstream InFileStream;
  google::protobuf::io::ZeroCopyInputStream *InZeroCopyStream = nullptr;
  google::protobuf::io::GzipInputStream *GzipStream = nullptr;
  google::protobuf::io::CodedInputStream *CodedStream = nullptr;
};

#endif