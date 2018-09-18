#ifndef LLVM_TDG_TDG_SERIALIZER_H
#define LLVM_TDG_TDG_SERIALIZER_H

#include "TDGInstruction.pb.h"

#include "DataGraph.h"
#include "DynamicInstruction.h"
#include "Gem5ProtobufSerializer.h"

/**
 * This class helps incremental serializes the tdg instructions
 * into protobuf.
 */

class TDGSerializer {
public:
  explicit TDGSerializer(const std::string &FileName);
  ~TDGSerializer();

  /**
   * Serialize the static information at the beginning.
   */
  void serializeStaticInfo(const LLVM::TDG::StaticInformation &StaticInfo);

  void serialize(DynamicInstruction *DynamicInst, DataGraph *DG);

private:
  Gem5ProtobufSerializer Gem5Serializer;

  // Flag static information serialized.
  bool SerializedStaticInfomation;

  uint64_t SerializedInsts;
  LLVM::TDG::TDG TDG;

  /**
   * Actually writing to the file.
   */
  void write();
};

#endif