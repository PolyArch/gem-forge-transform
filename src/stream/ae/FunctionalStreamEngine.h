#ifndef LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H
#define LLVM_TDG_FUNCTIONAL_STREAM_ENGINE_H

#include "DynamicStreamCoalescer.h"
#include "FunctionalStream.h"

#include <unordered_map>

class FunctionalStreamEngine {
 public:
  FunctionalStreamEngine(std::unique_ptr<llvm::Interpreter> &_Interpreter,
                         const std::unordered_set<Stream *> &_ChosenStreams);

  void configure(Stream *S, DataGraph *DG);
  void step(Stream *S, DataGraph *DG);

  void updateWithValue(Stream *S, DataGraph *DG,
                       const DynamicValue &DynamicVal);

  void endStream(Stream *S);
  void access(Stream *S);

  void endAll();

  void finalizeCoalesceInfo();

  std::unique_ptr<llvm::Interpreter> &getInterpreter() {
    return this->Interpreter;
  }

 private:
  std::unique_ptr<llvm::Interpreter> &Interpreter;
  std::unordered_map<const Stream *, FunctionalStream> StreamMap;
  std::unordered_map<const FunctionalStream *,
                     std::list<DynamicStreamCoalescer>>
      DynamicStreamCoalescerMap;
};

#endif