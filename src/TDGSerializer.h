#ifndef LLVM_TDG_TDG_SERIALIZER_H
#define LLVM_TDG_TDG_SERIALIZER_H

#include "TDGInstruction.pb.h"

#include "DataGraph.h"
#include "DynamicInstruction.h"

#include <google/protobuf/io/zero_copy_stream.h>

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
  // Underlying file output stream.
  std::ofstream OutFileStream;

  google::protobuf::io::ZeroCopyOutputStream *OutZeroCopyStream;

  uint64_t SerializedInsts;
  LLVM::TDG::TDG TDG;

  /**
   * Copied from gem5/src/proto/protoio.hh.
   * Write this magic number first so that it can be readed by
   * gem5's protoio.
   */
  /// Use the ASCII characters gem5 as our magic number
  static const uint32_t Gem5MagicNumber = 0x356d6567;

  /**
   * Actually writing to the file.
   */
  void write();
};

#endif