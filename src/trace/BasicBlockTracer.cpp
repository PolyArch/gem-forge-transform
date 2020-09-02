#include "BasicBlockTracer.h"

#include "../ProtobufSerializer.h"

void BasicBlockTracer::initialize(const char *fileName) {
  this->fileName = fileName;
}

void BasicBlockTracer::addBasicBlock(uint64_t uid) {
  if (this->currentBB.repeats() == 0) {
    // First One.
    this->currentBB.set_uid(uid);
    this->currentBB.set_repeats(1);
  } else if (this->currentBB.uid() != uid) {
    // Different BB.
    this->bufferedBB.push_back(this->currentBB);
    this->currentBB.set_uid(uid);
    this->currentBB.set_repeats(1);
  } else {
    // Repeated BB.
    this->currentBB.set_repeats(this->currentBB.repeats() + 1);
  }
  if (this->bufferedBB.size() > 10000000) {
    this->serializeToFile();
  }
}

void BasicBlockTracer::cleanup() {
  if (this->currentBB.repeats() > 0) {
    this->bufferedBB.push_back(this->currentBB);
    this->currentBB.set_repeats(0);
  }
  this->serializeToFile();
  delete this->serializer;
  this->serializer = nullptr;
}

void BasicBlockTracer::serializeToFile() {
  if (!this->serializer) {
    this->serializer = new GzipMultipleProtobufSerializer(this->fileName);
  }
  for (const auto &msg : this->bufferedBB) {
    this->serializer->serialize(msg);
  }
  this->bufferedBB.clear();
}