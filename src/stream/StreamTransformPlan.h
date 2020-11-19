#ifndef LLVM_TDG_STREAM_STREAM_TRANSFORM_PLAN_H
#define LLVM_TDG_STREAM_STREAM_TRANSFORM_PLAN_H

#include "stream/DynStream.h"

struct StreamTransformPlan {
public:
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

  const std::unordered_set<DynStream *> &getUsedStreams() const {
    return this->UsedStreams;
  }
  const std::unordered_set<DynStream *> &getStepStreams() const {
    return this->StepStreams;
  }
  DynStream *getParamStream() const { return this->ParamStream; }

  void addUsedStream(DynStream *UsedStream) {
    this->UsedStreams.insert(UsedStream);
  }

  void planToDelete() { this->Plan = DELETE; }
  void planToStep(DynStream *S) {
    this->StepStreams.insert(S);
    this->ParamStream = S;
    this->Plan = STEP;
  }
  void planToStore(DynStream *S) {
    this->ParamStream = S;
    this->Plan = STORE;
    // Store is also considered as use.
    this->addUsedStream(S);
  }

  std::string format() const;

private:
  std::unordered_set<DynStream *> UsedStreams;
  std::unordered_set<DynStream *> StepStreams;
  DynStream *ParamStream;
};

#endif