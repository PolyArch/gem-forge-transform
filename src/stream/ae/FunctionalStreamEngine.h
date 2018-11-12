#ifndef LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H
#define LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H

#include "DynamicStreamCoalescer.h"
#include "FunctionalStream.h"

#include <unordered_map>

class FunctionalStreamEngine {
public:
  FunctionalStreamEngine(std::unique_ptr<llvm::Interpreter> &_Interpreter);

  void configure(Stream *S, DataGraph *DG);
  void step(Stream *S, DataGraph *DG);
  void updateLoadedValue(Stream *S, DataGraph *DG,
                         const DynamicValue &DynamicVal);
  void updatePHINodeValue(Stream *S, DataGraph *DG,
                          const DynamicValue &DynamicVal);
  void endStream(Stream *S);
  void access(Stream *S);

  void finalizeCoalesceInfo();

  FunctionalStream *getNullableFunctionalStream(Stream *S);
  std::unique_ptr<llvm::Interpreter> &getInterpreter() {
    return this->Interpreter;
  }

private:
  std::unique_ptr<llvm::Interpreter> &Interpreter;
  std::unordered_map<Stream *, FunctionalStream> StreamMap;
  std::unordered_map<FunctionalStream *, DynamicStreamCoalescer>
      DynamicStreamCoalescerMap;

  FunctionalStream *getOrInitializeFunctionalStream(Stream *S);
  DynamicStreamCoalescer *
  getOrInitializeDynamicStreamCoalescer(FunctionalStream *FS);
};

#endif