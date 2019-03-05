#include "StreamTransformPlan.h"

#include <iomanip>
#include <sstream>

std::string StreamTransformPlan::format() const {
  std::stringstream ss;
  ss << std::setw(10) << std::left
     << StreamTransformPlan::formatPlanT(this->Plan);
  if (this->Plan == StreamTransformPlan::PlanT::STEP) {
    ss << '-';
    for (auto StepStream : this->StepStreams) {
      ss << StepStream->formatName();
    }
    ss << '-';
  }
  for (auto UsedStream : this->UsedStreams) {
    ss << UsedStream->formatName() << ' ';
  }
  return ss.str();
}