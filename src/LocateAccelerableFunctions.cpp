#include "LocateAccelerableFunctions.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "LocateAccelerableFunctions"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

void LocateAccelerableFunctions::getAnalysisUsage(
    llvm::AnalysisUsage &Info) const {
  Info.addRequired<llvm::CallGraphWrapperPass>();
  Info.setPreservesAll();
}

bool LocateAccelerableFunctions::runOnModule(llvm::Module &Module) {

  // Add some math accelerable functions.
  this->MathAcclerableFunctionNames.insert("sin");
  this->MathAcclerableFunctionNames.insert("cos");

  auto &CGPass = getAnalysis<llvm::CallGraphWrapperPass>();

  std::unordered_map<llvm::CallGraphNode *,
                     std::unordered_set<llvm::CallGraphNode *>>
      ReverseCG;

  // Reverse the callgraph.
  for (auto NodeIter = CGPass.begin(), NodeEnd = CGPass.end();
       NodeIter != NodeEnd; ++NodeIter) {
    llvm::CallGraphNode *Caller = NodeIter->second.get();
    ReverseCG.emplace(std::piecewise_construct, std::forward_as_tuple(Caller),
                      std::forward_as_tuple());
    for (auto CalleeIter = Caller->begin(), CalleeEnd = Caller->end();
         CalleeIter != CalleeEnd; ++CalleeIter) {
      // CalleeIter is a std::vector<CallRecord>::const_iterator.
      // CallRecord is a std::pair<WeakTrackingVH, CallGraphNode *>.
      llvm::CallGraphNode *Callee = CalleeIter->second;
      if (ReverseCG.find(Callee) == ReverseCG.end()) {
        ReverseCG.emplace(std::piecewise_construct,
                          std::forward_as_tuple(Callee),
                          std::forward_as_tuple());
      }
      ReverseCG.at(Callee).insert(Caller);
    }
  }

  // Propagates from CallsExternalNode.
  std::list<llvm::CallGraphNode *> Queue{CGPass.getCallsExternalNode()};
  std::unordered_set<llvm::CallGraphNode *> Visited;
  while (!Queue.empty()) {
    auto Node = Queue.front();
    Queue.pop_front();
    Visited.insert(Node);
    if (ReverseCG.find(Node) == ReverseCG.end()) {
      LLVM_DEBUG(llvm::dbgs() << "Missing node in reverse CG for " << '\n');
      continue;
    }
    // assert(ReverseCG.find(Node) != ReverseCG.end() &&
    //        "Missing node in reverse call graph.");
    for (auto Caller : ReverseCG.at(Node)) {
      if (Visited.find(Caller) != Visited.end()) {
        continue;
      }
      auto Function = Caller->getFunction();
      if (Function != nullptr &&
          this->isMathAccelerable(Function->getName().str())) {
        continue;
      }
      Queue.push_back(Caller);
    }
  }

  for (auto Node : Visited) {
    auto Function = Node->getFunction();
    if (Function != nullptr) {
      this->FunctionUnAccelerable.insert(Function->getName().str());
    }
  }

  return false;
}

uint64_t AccelerableFunctionInfo::countInstructionInFunction(
    const llvm::Function *Function) const {
  uint64_t Count = 0;
  for (auto BBIter = Function->begin(), BBEnd = Function->end();
       BBIter != BBEnd; ++BBIter) {
    Count += BBIter->size();
  }
  return Count;
}

void AccelerableFunctionInfo::countLoopInCurrentFunction(
    uint64_t &TopLevelLoopCount, uint64_t &AllLoopCount) {
  auto &LoopInfo = this->getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();
  TopLevelLoopCount = 0;
  AllLoopCount = LoopInfo.getLoopsInPreorder().size();
  for (auto LoopIter = LoopInfo.begin(), LoopEnd = LoopInfo.end();
       LoopIter != LoopEnd; ++LoopIter) {
    TopLevelLoopCount++;
  }
}

void AccelerableFunctionInfo::getAnalysisUsage(
    llvm::AnalysisUsage &Info) const {
  Info.addRequired<LocateAccelerableFunctions>();
  Info.addRequired<llvm::LoopInfoWrapperPass>();
  Info.setPreservesAll();
}

bool AccelerableFunctionInfo::doInitialization(llvm::Module &Module) {
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

bool AccelerableFunctionInfo::runOnFunction(llvm::Function &Function) {
  bool Accelerable =
      this->getAnalysis<LocateAccelerableFunctions>().isAccelerable(
          Function.getName().str());

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

bool AccelerableFunctionInfo::doFinalization(llvm::Module &Module) {
  LLVM_DEBUG(llvm::dbgs() << "AFI: FunctionCount               "
                          << this->FunctionCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: AccelerableFunctionCount    "
                          << this->AccelerableFunctionCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: InstructionCount            "
                          << this->InstructionCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: AccelerableInstructionCount "
                          << this->AccelerableInstructionCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: TopLoopCount                "
                          << this->TopLevelLoopCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: AccelerableTopLoopCount     "
                          << this->AccelerableTopLevelLoopCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: AllLoopCount                "
                          << this->AllLoopCount << '\n');
  LLVM_DEBUG(llvm::dbgs() << "AFI: AccelerableAllLoopCount     "
                          << this->AccelerableAllLoopCount << '\n');
  return false;
}

#undef DEBUG_TYPE

char LocateAccelerableFunctions::ID = 0;
static llvm::RegisterPass<LocateAccelerableFunctions>
    L("locate-acclerable-functions", "Locate accelerable functions", false,
      true);
char AccelerableFunctionInfo::ID = 0;
static llvm::RegisterPass<AccelerableFunctionInfo>
    X("acclerable-function-info", "Accelerable function info", false, true);