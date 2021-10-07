#include "BasicBlockTracer.h"

#include "../ProtobufSerializer.h"

void BasicBlockTracer::initialize(const char *fileName) {
  this->fileName = fileName;
}

void BasicBlockTracer::addInst(uint64_t uid) {
  this->bufferedBB.push_back(uid);
  if (this->bufferedBB.size() > 10000000) {
    this->serializeToFile();
  }
}

void BasicBlockTracer::cleanup() {
  this->serializeToFile();
  delete this->serializer;
  this->serializer = nullptr;
}

void BasicBlockTracer::serializeToFile() {
  if (!this->serializer) {
    this->serializer = new GzipMultipleProtobufSerializer(this->fileName);
  }
  for (const auto &uid : this->bufferedBB) {
    this->serializer->writeVarint64(uid);
  }
  this->bufferedBB.clear();
}