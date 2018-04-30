#include "LocateAccelerableFunctions.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "LocateAccelerableFunctions"

void LocateAccelerableFunctions::getAnalysisUsage(
    llvm::AnalysisUsage& Info) const {
  //   Info.addRequired<llvm::LoopInfoWrapperPass>();
  Info.setPreservesAll();
}

bool LocateAccelerableFunctions::doInitialization(llvm::CallGraph& CG) {
  // Add some math accelerable functions.
  this->MathAcclerableFunctionNames.insert("sin");
  this->MathAcclerableFunctionNames.insert("cos");

  // CG.dump();
  this->CallsExternalNode = CG.getCallsExternalNode();
  this->ExternalCallingNode = CG.getExternalCallingNode();

  return false;
}

bool LocateAccelerableFunctions::runOnSCC(llvm::CallGraphSCC& SCC) {
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

    // Still we need to ignore the ExternalCallingNode and
    // CallsExternalNode.
    if (Caller == this->ExternalCallingNode ||
        Caller == this->CallsExternalNode) {
      continue;
    }

    auto Function = Caller->getFunction();
    auto CallerName = Function->getName().str();
    DEBUG(llvm::errs() << CallerName << " is accelerable? "
                       << (Accelerable ? "true" : "false") << '\n');
    this->MapFunctionAccelerable[CallerName] = Accelerable;

    // Update statistics.
    // We only count those function with body we known.
    if (Function->size() == 0) {
      // We don't have the definition?
      continue;
    }
  }
  return false;
}

uint64_t AccelerableFunctionInfo::countInstructionInFunction(
    const llvm::Function* Function) const {
  uint64_t Count = 0;
  for (auto BBIter = Function->begin(), BBEnd = Function->end();
       BBIter != BBEnd; ++BBIter) {
    Count += BBIter->size();
  }
  return Count;
}

void AccelerableFunctionInfo::countLoopInCurrentFunction(
    uint64_t& TopLevelLoopCount, uint64_t& AllLoopCount) {
  auto& LoopInfo = this->getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();
  TopLevelLoopCount = 0;
  AllLoopCount = LoopInfo.getLoopsInPreorder().size();
  for (auto LoopIter = LoopInfo.begin(), LoopEnd = LoopInfo.end();
       LoopIter != LoopEnd; ++LoopIter) {
    TopLevelLoopCount++;
  }
}

void AccelerableFunctionInfo::getAnalysisUsage(
    llvm::AnalysisUsage& Info) const {
  Info.addRequired<LocateAccelerableFunctions>();
  Info.addRequired<llvm::LoopInfoWrapperPass>();
  Info.setPreservesAll();
}

bool AccelerableFunctionInfo::doInitialization(llvm::Module& Module) {
  this->FunctionCount = 0;
  this->AccelerableFunctionCount = 0;
  this->InstructionCount = 0;
  this->AccelerableInstructionCount = 0;
  this->TopLevelLoopCount = 0;
  this->AccelerableTopLevelLoopCount = 0;
  this->AllLoopCount = 0;
  this->AccelerableAllLoopCount = 0;
  return false;
}

bool AccelerableFunctionInfo::runOnFunction(llvm::Function& Function) {
  bool Accelerable =
      this->getAnalysis<LocateAccelerableFunctions>().isAccelerable(
          Function.getName());

  uint64_t InstructionInFunctionCount =
      this->countInstructionInFunction(&Function);

  uint64_t TopLevelLoopInFunctionCount = 0;
  uint64_t AllLoopInFunctionCount = 0;
  this->countLoopInCurrentFunction(TopLevelLoopInFunctionCount,
                                   AllLoopInFunctionCount);
  this->FunctionCount++;
  this->InstructionCount += InstructionInFunctionCount;
  this->TopLevelLoopCount += TopLevelLoopInFunctionCount;
  this->AllLoopCount += AllLoopInFunctionCount;

  if (Accelerable) {
    this->AccelerableFunctionCount++;
    this->AccelerableInstructionCount += InstructionInFunctionCount;
    this->AccelerableTopLevelLoopCount += TopLevelLoopInFunctionCount;
    this->AccelerableAllLoopCount += AllLoopInFunctionCount;
  }

  return false;
}

bool AccelerableFunctionInfo::doFinalization(llvm::Module& Module) {
  DEBUG(llvm::errs() << "AFI: FunctionCount               "
                     << this->FunctionCount << '\n');
  DEBUG(llvm::errs() << "AFI: AccelerableFunctionCount    "
                     << this->AccelerableFunctionCount << '\n');
  DEBUG(llvm::errs() << "AFI: InstructionCount            "
                     << this->InstructionCount << '\n');
  DEBUG(llvm::errs() << "AFI: AccelerableInstructionCount "
                     << this->AccelerableInstructionCount << '\n');
  DEBUG(llvm::errs() << "AFI: TopLoopCount                "
                     << this->TopLevelLoopCount << '\n');
  DEBUG(llvm::errs() << "AFI: AccelerableTopLoopCount     "
                     << this->AccelerableTopLevelLoopCount << '\n');
  DEBUG(llvm::errs() << "AFI: AllLoopCount                "
                     << this->AllLoopCount << '\n');
  DEBUG(llvm::errs() << "AFI: AccelerableAllLoopCount     "
                     << this->AccelerableAllLoopCount << '\n');
  return false;
}

#undef DEBUG_TYPE

char LocateAccelerableFunctions::ID = 0;
static llvm::RegisterPass<LocateAccelerableFunctions> L(
    "locate-acclerable-functions", "Locate accelerable functions", false, true);
char AccelerableFunctionInfo::ID = 0;
static llvm::RegisterPass<AccelerableFunctionInfo> X(
    "acclerable-function-info", "Accelerable function info", false, true);