#ifndef LLVM_TDG_LOCATE_ACCELERABLE_FUNCTIONS_H
#define LLVM_TDG_LOCATE_ACCELERABLE_FUNCTIONS_H

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include <map>
#include <set>

// A analysis pass to locate all the accelerable functions.
// A function is accelerable if:
// 1. it has no system call.
// 2. the function it calls is also acclerable.
// In short, we can trace these functions and that's why they are accelerable.
#define DEBUG_TYPE "LocateAccelerableFunctionsPass"
class LocateAccelerableFunctions : public llvm::CallGraphSCCPass {
 public:
  static char ID;
  LocateAccelerableFunctions()
      : llvm::CallGraphSCCPass(ID), CallsExternalNode(nullptr) {}
  bool doInitialization(llvm::CallGraph& CG) override {
    // Add some math accelerable functions.
    this->MathAcclerableFunctionNames.insert("sin");
    this->MathAcclerableFunctionNames.insert("cos");
    // We do not modify the module.
    CG.dump();
    this->CallsExternalNode = CG.getCallsExternalNode();
    this->ExternalCallingNode = CG.getExternalCallingNode();
    return false;
  }
  bool runOnSCC(llvm::CallGraphSCC& SCC) override {
    // If any function in this SCC calls an external node or unaccelerable
    // functions, then it is not accelerable.
    bool Accelerable = true;
    for (auto NodeIter = SCC.begin(), NodeEnd = SCC.end(); NodeIter != NodeEnd;
         ++NodeIter) {
      // NodeIter is a std::vector<CallGraphNode *>::const_iterator.
      llvm::CallGraphNode* Caller = *NodeIter;
      if (Caller == this->ExternalCallingNode ||
          Caller == this->CallsExternalNode) {
        // Special case: ignore ExternalCallingNode and CallsExternalNode.
        return false;
      }
      auto CallerName = Caller->getFunction()->getName().str();
      // By pass checking of all the math accelerable functions.
      if (this->isMathAccelerable(CallerName)) {
        continue;
      }
      // Check this function doesn't call external methods.
      for (auto CalleeIter = Caller->begin(), CalleeEnd = Caller->end();
           CalleeIter != CalleeEnd; ++CalleeIter) {
        // CalleeIter is a std::vector<CallRecord>::const_iterator.
        // CallRecord is a std::pair<WeakTrackingVH, CallGraphNode *>.
        llvm::CallGraphNode* Callee = CalleeIter->second;
        // Calls external node.
        if (Callee == this->CallsExternalNode) {
          //   DEBUG(llvm::errs() << CallerName << " calls external node\n");
          Accelerable = false;
          break;
        }
        // Calls unaccelerable node.
        auto CalleeName = Callee->getFunction()->getName().str();
        if (this->MapFunctionAccelerable.find(CalleeName) !=
                this->MapFunctionAccelerable.end() &&
            this->MapFunctionAccelerable[CalleeName] == false) {
          //   DEBUG(llvm::errs() << CallerName << " calls unaccelerable
          //   function "
          //                      << CalleeName << '\n');
          Accelerable = false;
          break;
        }
      }
      if (!Accelerable) {
        break;
      }
    }
    // Add these functions to the map.
    for (auto CallerIter = SCC.begin(), CallerEnd = SCC.end();
         CallerIter != CallerEnd; ++CallerIter) {
      llvm::CallGraphNode* Caller = *CallerIter;
      auto CallerName = Caller->getFunction()->getName().str();
      if (Accelerable) {
        DEBUG(llvm::errs() << CallerName << " is accelerable? "
                           << (Accelerable ? "true" : "false") << '\n');
      }
      this->MapFunctionAccelerable[CallerName] = Accelerable;
    }
    return false;
  }
  bool isAccelerable(const std::string& FunctionName) const {
    return this->MapFunctionAccelerable.find(FunctionName) !=
               this->MapFunctionAccelerable.end() &&
           this->MapFunctionAccelerable.at(FunctionName);
  }

 private:
  // This represents an external function been called.
  llvm::CallGraphNode* CallsExternalNode;
  // This represents an external function calling to internal functions.
  llvm::CallGraphNode* ExternalCallingNode;

  std::map<std::string, bool> MapFunctionAccelerable;

  std::set<std::string> MathAcclerableFunctionNames;

  // Some special math function we always want to mark as accelerable,
  // even if we don't have the trace.
  // TODO: Use a map for this.
  bool isMathAccelerable(const std::string& FunctionName) {
    return this->MathAcclerableFunctionNames.find(FunctionName) !=
           this->MathAcclerableFunctionNames.end();
  }
};
#undef DEBUG_TYPE

#endif