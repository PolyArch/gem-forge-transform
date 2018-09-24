#ifndef LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H
#define LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H

#include "FunctionalStream.h"

#include <unordered_map>

class FunctionalStreamEngine {
public:
  FunctionalStreamEngine(std::unique_ptr<llvm::Interpreter> &_Interpreter);

  void configure(Stream *S, DataGraph *DG);
  void step(Stream *S, DataGraph *DG);
  void updateLoadedValue(Stream *S, DataGraph *DG,
                         const DynamicValue &DynamicVal);
  void endStream(Stream *S);

  FunctionalStream *getNullableFunctionalStream(Stream *S);
  std::unique_ptr<llvm::Interpreter> &getInterpreter() {
    return this->Interpreter;
  }

private:
  std::unique_ptr<llvm::Interpreter> &Interpreter;
  std::unordered_map<Stream *, FunctionalStream> StreamMap;

  FunctionalStream *getOrInitializeFunctionalStream(Stream *S);
};

#endif