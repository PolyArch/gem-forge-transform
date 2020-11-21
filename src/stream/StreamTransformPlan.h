#ifndef LLVM_TDG_STREAM_STREAM_TRANSFORM_PLAN_H
#define LLVM_TDG_STREAM_STREAM_TRANSFORM_PLAN_H

#include "stream/StaticStream.h"

struct StreamTransformPlan {
public:
  using StreamSet = StaticStream::StreamSet;
  enum PlanT {
    NOTHING,
    DELETE,
    STEP,
    STORE,
    LOAD_STEP,
  } Plan;

  static std::string formatPlanT(const PlanT Plan) {
    switch (Plan) {
    case NOTHING:
      return "NOTHING";
    case DELETE:
      return "DELETE";
    case STEP:
      return "STEP";
    case STORE:
      return "STORE";
    case LOAD_STEP:
      return "LOAD_STEP";
    }
    llvm_unreachable("Illegal StreamTransformPlan::PlanT to be formatted.");
  }

  StreamTransformPlan() : Plan(NOTHING) {}

  const StreamSet &getUsedStreams() const { return this->UsedStreams; }
  const StreamSet &getStepStreams() const { return this->StepStreams; }
  StaticStream *getParamStream() const { return this->ParamStream; }

  void addUsedStream(StaticStream *UsedStream) {
    this->UsedStreams.insert(UsedStream);
  }

  void planToDelete() { this->Plan = DELETE; }
  void planToStep(StaticStream *S) {
    this->StepStreams.insert(S);
    this->ParamStream = S;
    this->Plan = STEP;
  }
  void planToStore(StaticStream *S) {
    this->ParamStream = S;
    this->Plan = STORE;
    // Store is also considered as use.
    this->addUsedStream(S);
  }

  std::string format() const;

private:
  std::unordered_set<StaticStream *> UsedStreams;
  std::unordered_set<StaticStream *> StepStreams;
  StaticStream *ParamStream;
};

#endif