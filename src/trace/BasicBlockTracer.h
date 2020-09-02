#ifndef LLVM_TDG_BASIC_BLOCK_TRACER_H
#define LLVM_TDG_BASIC_BLOCK_TRACER_H

#include "BasicBlockTraceMessage.pb.h"

#include <vector>

// A thin wrapper over the LLVM::TDG::Profile structure.
// This can only be used for single thread.
class GzipMultipleProtobufSerializer;
class BasicBlockTracer {
public:
  // Initialize with the TraceFileName.
  void initialize(const char *fileName);

  // Increase the counter of the specific basic block.
  void addBasicBlock(uint64_t uid);

  // Serialize the trace results to a file.
  void cleanup();

private:
  LLVM::TDG::DynamicLLVMBasicBlock currentBB;
  std::vector<LLVM::TDG::DynamicLLVMBasicBlock> bufferedBB;
  const char *fileName = nullptr;
  GzipMultipleProtobufSerializer *serializer = nullptr;

  void serializeToFile();
};
#endif