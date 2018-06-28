#ifndef LLVM_TDG_TDG_SERIALIZER_H
#define LLVM_TDG_TDG_SERIALIZER_H

#include "TDGInstruction.pb.h"

#include "DataGraph.h"
#include "DynamicInstruction.h"

#include <fstream>
#include <string>

/**
 * This class helps incremental serializes the tdg instructions
 * into protobuf.
 */

class TDGSerializer {
public:
  explicit TDGSerializer(const std::string &FileName);
  ~TDGSerializer();

  void serialize(DynamicInstruction *DynamicInst, DataGraph *DG);

private:
  std::ofstream Out;
  uint64_t SerializedInsts;
  LLVM::TDG::TDG TDG;
};

#endif