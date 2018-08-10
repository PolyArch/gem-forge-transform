#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include "LocateAccelerableFunctions.h"
#include "trace/Tracer.h"

#include <map>
#include <set>
#include <unordered_set>

static llvm::cl::opt<std::string>
    TraceFunctionNames("trace-function",
                       llvm::cl::desc("Trace function names."));
static llvm::cl::opt<bool>
    TraceInstructionOnly("trace-inst-only",
                         llvm::cl::desc("Trace instruction only."));

/**
 * Some times when we try to trace into stdlib, we want to reduce the overhead
 * of the tracer runtime. We want to limit our choice of traced functions.
 * This requires the function call graphs are determinable. If we ever encounter
 * an indirect call, we abort.
 * This option will be ingored if "trace-function" is not specified.
 */
static llvm::cl::opt<bool> TraceReachableFunctionOnly(
    "trace-reachable-only",
    llvm::cl::desc("Trace reachable functions from trace-function "
                   "only. About when there is indirect call."));

#define DEBUG_TYPE "TracePass"
namespace {

bool isIntrinsicFunctionName(const std::string &FunctionName) {
  if (FunctionName.find("llvm.") == 0) {
    // Hack: return true if the name start with llvm.
    return true;
  } else {
    return false;
  }
}

llvm::Constant *
getOrCreateStringLiteral(std::map<std::string, llvm::Constant *> &GlobalStrings,
                         llvm::Module *Module, const std::string &Str) {
  if (GlobalStrings.find(Str) == GlobalStrings.end()) {
    // Create the constant array.
    auto Array =
        llvm::ConstantDataArray::getString(Module->getContext(), Str, true);
    // Get the array type.
    auto ElementTy = llvm::IntegerType::get(Module->getContext(), 8);
    auto ArrayTy = llvm::ArrayType::get(ElementTy, Str.size() + 1);
    // Register a global variable.
    auto GlobalVariableStr = new llvm::GlobalVariable(
        *(Module), ArrayTy, true, llvm::GlobalValue::PrivateLinkage, Array,
        ".str");
    GlobalVariableStr->setAlignment(1);
    /**
     * Get the address of the first element in the string.
     * Notice that the global variable %.str is also a pointer, that's why we
     * need two indexes. See great tutorial:
     * https://
     * llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction
     */
    auto ConstantZero = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(Module->getContext()), 0, false);
    std::vector<llvm::Constant *> Indexes;
    Indexes.push_back(ConstantZero);
    Indexes.push_back(ConstantZero);
    GlobalStrings[Str] = llvm::ConstantExpr::getGetElementPtr(
        ArrayTy, GlobalVariableStr, Indexes);
  }
  return GlobalStrings.at(Str);
}

llvm::Value *getOrCreateVectorStoreBuffer(
    std::map<unsigned int, llvm::Value *> &VectorStoreBuffer,
    llvm::Function *Function, llvm::VectorType *Type) {
  auto BitSize = Type->getBitWidth();
  if (VectorStoreBuffer.find(BitSize) == VectorStoreBuffer.end()) {
    // Insert at the beginning of the function.
    auto IP = Function->front().getFirstInsertionPt();
    auto Instruction = &*IP;
    for (auto Iter = Function->front().begin(), End = Function->front().end();
         Iter != End; ++Iter) {
      DEBUG(llvm::errs() << '(' << Iter->getName() << ", "
                         << Iter->getOpcodeName() << ")\n");
    }
    DEBUG(llvm::errs() << "Found insertion point for vector buffer in "
                       << Function->getName()
                       << "::" << Function->front().getName() << " at ("
                       << IP->getName() << ", " << IP->getOpcodeName()
                       << ")\n");
    llvm::IRBuilder<> Builder(IP->getParent(), IP);
    llvm::Value *Alloca = Builder.CreateAlloca(Type);
    for (auto Iter = Function->front().begin(), End = Function->front().end();
         Iter != End; ++Iter) {
      DEBUG(llvm::errs() << '(' << Iter->getName() << ", "
                         << Iter->getOpcodeName() << ")\n");
    }
    VectorStoreBuffer[BitSize] = Alloca;
  }

  return VectorStoreBuffer.at(BitSize);
}

// This is a pass to instrument a function and trace it.
class TracePass : public llvm::FunctionPass {
public:
  static char ID;
  TracePass() : llvm::FunctionPass(ID) {}
  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    // DEBUG(llvm::errs() << "We have required the analysis.\n");
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
  }
  bool doInitialization(llvm::Module &Module) override {
    this->Module = &Module;

    DEBUG(llvm::errs() << "Initialize TracePass\n");

    // Register the external log functions.
    registerPrintFunctions(Module);

    // If trace function specified, find all the reachable functions.
    // Parse the comma separated list.
    if (TraceFunctionNames.getNumOccurrences() == 1) {
      size_t Prev = 0;
      // Append a comma to eliminate a corner case.
      std::string Names = TraceFunctionNames.getValue() + ',';
      for (size_t Curr = 0; Curr < Names.size(); ++Curr) {
        if (Names[Curr] == ',') {
          auto Iter =
              this->tracedFunctionNames.insert(Names.substr(Prev, Curr - Prev))
                  .first;
          Prev = Curr + 1;
          DEBUG(llvm::errs() << "Add traced function " << *Iter << '\n');
        }
      }
      if (TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
          TraceReachableFunctionOnly.getValue()) {
        this->findReachableFunctions();
      }
    }

    return true;
  }
  bool runOnFunction(llvm::Function &Function) override {
    auto FunctionName = Function.getName().str();
    DEBUG(llvm::errs() << "Processing Function: " << FunctionName << '\n');

    if (TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
        TraceReachableFunctionOnly.getValue()) {
      if (this->reachableFunctions.find(&Function) ==
          this->reachableFunctions.end()) {
        // This function is not reachable.
        // We safely ignore it.
        return false;
      }
    }

    // Set the current function.
    this->CurrentFunction = &Function;
    // Clear the vector store buffer.
    this->VectorStoreBuffer.clear();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      // DEBUG(llvm::errs() << "Found basic block: " << BBIter->getName() <<
      // '\n');
      runOnBasicBlock(*BBIter);
    }

    // Trace the function args.
    traceFuncArgs(Function);

    // We always modify the function.
    return true;
  }

private:
  llvm::Module *Module;
  llvm::Function *CurrentFunction;
  std::map<unsigned int, llvm::Value *> VectorStoreBuffer;

  /* Print functions. */
  llvm::Value *PrintInstFunc;
  llvm::Value *PrintValueFunc;
  llvm::Value *PrintFuncEnterFunc;
  llvm::Value *PrintInstEndFunc;
  llvm::Value *PrintFuncEnterEndFunc;

  // Contains all the traced functions, if specified.
  std::unordered_set<std::string> tracedFunctionNames;

  /**
   * Store the reachable function from tracedFunctionNames.
   */
  std::unordered_set<llvm::Function *> reachableFunctions;

  void findReachableFunctions();

  // Insert all the print function declaration into the module.
  void registerPrintFunctions(llvm::Module &Module) {
    auto &Context = Module.getContext();
    auto VoidTy = llvm::Type::getVoidTy(Context);
    auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
    auto Int8Ty = llvm::Type::getInt8Ty(Context);
    auto Int32Ty = llvm::Type::getInt32Ty(Context);

    std::vector<llvm::Type *> PrintInstArgs{
        Int8PtrTy, // char* FunctionName
        Int8PtrTy, // char* BBName,
        Int32Ty,   // unsigned Id,
        Int8PtrTy, // char* OpCodeName,
    };
    auto PrintInstTy = llvm::FunctionType::get(VoidTy, PrintInstArgs, false);
    this->PrintInstFunc = Module.getOrInsertFunction("printInst", PrintInstTy);

    std::vector<llvm::Type *> PrintValueArgs{
        Int8Ty,
        Int8PtrTy, // char* Name,
        Int32Ty,
        Int32Ty,
    };
    auto PrintValueTy = llvm::FunctionType::get(VoidTy, PrintValueArgs, true);
    this->PrintValueFunc =
        Module.getOrInsertFunction("printValue", PrintValueTy);

    std::vector<llvm::Type *> PrintFuncEnterArgs{
        Int8PtrTy,
        Int32Ty,
    };
    auto PrintFuncEnterTy =
        llvm::FunctionType::get(VoidTy, PrintFuncEnterArgs, false);
    this->PrintFuncEnterFunc =
        Module.getOrInsertFunction("printFuncEnter", PrintFuncEnterTy);

    auto PrintInstEndTy = llvm::FunctionType::get(VoidTy, false);
    this->PrintInstEndFunc =
        Module.getOrInsertFunction("printInstEnd", PrintInstEndTy);

    auto PrintFuncEnterEndTy = llvm::FunctionType::get(VoidTy, false);
    this->PrintFuncEnterEndFunc =
        Module.getOrInsertFunction("printFuncEnterEnd", PrintFuncEnterEndTy);
  }

  std::map<std::string, llvm::Constant *> GlobalStrings;

  bool traceFuncArgs(llvm::Function &Function) {
    auto &EntryBB = Function.getEntryBlock();

    // Be careful here, as we may have introduced some alloc instruction
    // at the beginning for vector buffer, we have to start insertion
    // after these allocation instructions.
    // Hack, we can simply use the size of the VectorStoreBuffer to estimate
    // the number of alloca instructions we have to skip.
    auto IP = EntryBB.getFirstInsertionPt();
    for (size_t Count = 0; Count < this->VectorStoreBuffer.size(); ++Count) {
      assert(IP != EntryBB.end() &&
             "Too many alloca instructions for vector buffer.");
      IP++;
    }
    llvm::IRBuilder<> Builder(IP->getParent(), IP);

    bool IsTraced = true;
    if (this->tracedFunctionNames.size() != 0) {
      // The user has specified the traced functions.
      std::string FunctionName = Function.getName();
      IsTraced = this->tracedFunctionNames.find(FunctionName) !=
                 this->tracedFunctionNames.end();
    }
    auto IsTracedValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        (IsTraced ? 1 : 0), false);

    auto PrintFuncEnterArgs = std::vector<llvm::Value *>{
        getOrCreateStringLiteral(this->GlobalStrings, this->Module,
                                 Function.getName()),
        IsTracedValue,
    };
    Builder.CreateCall(this->PrintFuncEnterFunc, PrintFuncEnterArgs);
    for (auto ArgsIter = Function.arg_begin(), ArgsEnd = Function.arg_end();
         ArgsIter != ArgsEnd; ArgsIter++) {
      auto PrintValueArgs =
          getPrintValueArgs(PRINT_VALUE_TAG_PARAMETER, &*ArgsIter, Builder);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }
    Builder.CreateCall(this->PrintFuncEnterEndFunc);
    return true;
  }

  bool runOnBasicBlock(llvm::BasicBlock &BB) {
    llvm::BasicBlock::iterator NextInstIter;

    auto FunctionNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, BB.getParent()->getName());
    auto BBNameValue = getOrCreateStringLiteral(this->GlobalStrings,
                                                this->Module, BB.getName());
    auto PHIInsts = std::vector<std::pair<llvm::PHINode *, unsigned>>();
    unsigned InstId = 0;
    for (auto InstIter = BB.begin(); InstIter != BB.end();
         InstIter = NextInstIter, InstId++) {
      NextInstIter = InstIter;
      NextInstIter++;

      std::string OpCodeName = InstIter->getOpcodeName();
      // DEBUG(llvm::errs() << "Found instructions: " << OpCodeName << '\n');
      llvm::Instruction *Inst = &*InstIter;

      if (auto *PHI = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        // Special case for phi node as they must be grouped at the top
        // of the basic block.
        // Push them into a vector for later processing.
        PHIInsts.emplace_back(PHI, InstId);
        continue;
      }

      // Ignore the landpadinst.
      if (llvm::isa<llvm::LandingPadInst>(Inst)) {
        continue;
      }

      traceNonPhiInst(Inst, FunctionNameValue, BBNameValue, InstId);
    }

    // Handle all the phi nodes now.
    if (!PHIInsts.empty()) {
      auto InsertBefore = PHIInsts[PHIInsts.size() - 1].first->getNextNode();
      // Ignore landingpadinst.
      while (llvm::isa<llvm::LandingPadInst>(InsertBefore)) {
        InsertBefore = InsertBefore->getNextNode();
      }
      for (auto &PHIInstPair : PHIInsts) {
        auto PHIInst = PHIInstPair.first;
        auto InstId = PHIInstPair.second;
        tracePhiInst(PHIInst, FunctionNameValue, BBNameValue, InstId,
                     InsertBefore);
      }
    }

    return true;
  }

  // Phi node has to be handled specially as it must be the first
  // instruction of a basic block.
  // All the trace functions will be inserted beter @InsertBefore.
  void tracePhiInst(llvm::PHINode *Inst, llvm::Constant *FunctionNameValue,
                    llvm::Constant *BBNameValue, unsigned InstId,
                    llvm::Instruction *InsertBefore) {
    std::vector<llvm::Value *> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, BBNameValue, InstId);
    // Call printInst. After the instruction.
    // DEBUG(llvm::errs() << "Insert printInst for phi node.\n");
    llvm::IRBuilder<> Builder(InsertBefore);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    if (!TraceInstructionOnly.getValue()) {
      // Call printValue for each parameter before the instruction.
      for (unsigned IncomingValueId = 0,
                    NumIncomingValues = Inst->getNumIncomingValues();
           IncomingValueId < NumIncomingValues; IncomingValueId++) {
        // DEBUG(llvm::errs() << "Insert printValue for phi node.\n");
        auto PrintValueArgs = getPrintValueArgsForPhiParameter(
            Inst->getIncomingValue(IncomingValueId),
            Inst->getIncomingBlock(IncomingValueId));
        Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
      }

      // Call printResult after the instruction (if it has a result).
      if (Inst->getName() != "") {
        // DEBUG(llvm::errs() << "Insert printResult for phi node.\n");
        auto PrintValueArgs =
            getPrintValueArgs(PRINT_VALUE_TAG_RESULT, Inst, Builder);
        Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
      }
    }
    Builder.CreateCall(this->PrintInstEndFunc);
  }

  // Insert the trace call to non phi instructions.
  void traceNonPhiInst(llvm::Instruction *Inst,
                       llvm::Constant *FunctionNameValue,
                       llvm::Constant *BBNameValue, unsigned InstId) {
    std::string OpCodeName = Inst->getOpcodeName();
    assert(OpCodeName != "phi" && "traceNonPhiInst can't trace phi inst.");

    // DEBUG(llvm::errs() << "Trace non-phi inst " << Inst->getName() << " op "
    //                    << Inst->getOpcodeName() << '\n');

    std::vector<llvm::Value *> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, BBNameValue, InstId);
    // Call printInst. Before the instruction.
    llvm::IRBuilder<> Builder(Inst);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    if (!TraceInstructionOnly) {
      // Call printValue for each parameter before the instruction.
      for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
           OperandId < NumOperands; OperandId++) {
        auto PrintValueArgs = getPrintValueArgs(
            PRINT_VALUE_TAG_PARAMETER, Inst->getOperand(OperandId), Builder);

        if (Inst->getParent()->getParent()->getName() == "cargf") {
          DEBUG(llvm::errs() << "Trace non-phi inst " << Inst->getName()
                             << " op " << Inst->getOpcodeName() << '\n');
          for (auto Iter = Inst->getParent()->begin(),
                    End = Inst->getParent()->end();
               Iter != End; ++Iter) {
            DEBUG(llvm::errs() << '(' << Iter->getName() << ", "
                               << Iter->getOpcodeName() << ")\n");
            if (auto CallIter = llvm::dyn_cast<llvm::CallInst>(&*Iter)) {
              if (CallIter->getCalledFunction() != nullptr) {
                DEBUG(llvm::errs() << CallIter->getCalledFunction()->getName());
              }
            }
          }
        }

        Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
      }

      // Call printResult after the instruction (if it has a result).
      bool shouldTraceResult = Inst->getName() != "";
      // Special case for call: if the callee is traced, then we do
      // not log the result, but rely on the traced ret to provide it.
      // Otherwise, we log the result.
      if (shouldTraceResult) {
        if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
          auto CalledFunction = CallInst->getCalledFunction();
          if (CalledFunction == nullptr) {
            // This is a indirect call...
            // I just assume the provided handle is traced...
            shouldTraceResult = false;
          } else {
            if (!CalledFunction->isDeclaration()) {
              // The callee is traced.
              shouldTraceResult = false;
            }
          }
        } else if (auto InvokeInst = llvm::dyn_cast<llvm::InvokeInst>(Inst)) {
          /**
           * LLVM requires a terminator for each basic block.
           * Most of the terminators does not produce values,
           * thus no need to trace the result.
           * However, the only exception is 'invoke'. For now
           * I don't have any good solution to handle this
           * without messing a lot of code.
           * This should be fine for standalone mode.
           */
          shouldTraceResult = false;
        }
      }

      if (shouldTraceResult) {
        llvm::Instruction *NextInst = Inst->getNextNode();
        if (NextInst == nullptr) {
          assert(false && "Missing tracing last inst in block.");
        } else {
          assert(NextInst != nullptr && "Null next inst.");
          Builder.SetInsertPoint(NextInst);
          auto PrintValueArgs =
              getPrintValueArgs(PRINT_VALUE_TAG_RESULT, Inst, Builder);
          Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
        }
      }
    }
    // Call printInstEnd to commit.
    Builder.CreateCall(this->PrintInstEndFunc);
  }

  std::vector<llvm::Value *> getPrintInstArgs(llvm::Instruction *Inst,
                                              llvm::Constant *FunctionNameValue,
                                              llvm::Constant *BBNameValue,
                                              unsigned InstId) {
    std::string OpCodeName = Inst->getOpcodeName();

    // Create the string literal.
    auto OpCodeNameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, OpCodeName);

    auto InstIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()), InstId,
        false);

    std::vector<llvm::Value *> PrintInstArgs;
    PrintInstArgs.push_back(FunctionNameValue);
    PrintInstArgs.push_back(BBNameValue);
    PrintInstArgs.push_back(InstIdValue);
    PrintInstArgs.push_back(OpCodeNameValue);
    return std::move(PrintInstArgs);
  }

  // Get arguments from printValue to print phi node parameter.
  // Note that we can not get the run time value for incoming values.
  // Solution: print the incoming basic block and associated incoming
  // value's name.
  std::vector<llvm::Value *>
  getPrintValueArgsForPhiParameter(llvm::Value *IncomingValue,
                                   llvm::BasicBlock *IncomingBlock) {
    auto TagValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt8Ty(this->Module->getContext()),
        PRINT_VALUE_TAG_PARAMETER, false);
    auto Name = IncomingBlock->getName();
    auto NameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, Name);

    auto Type = IncomingBlock->getType();
    auto TypeId = Type->getTypeID();
    assert(TypeId == llvm::Type::TypeID::LabelTyID &&
           "Incoming block must have label type.");
    auto TypeIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()), TypeId,
        false);

    unsigned NumAdditionalArgs = 1;
    auto NumAdditionalArgsValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        NumAdditionalArgs, false);

    auto IncomingValueName = IncomingValue->getName();
    auto IncomingValueNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, IncomingValueName);

    std::vector<llvm::Value *> Args{TagValue, NameValue, TypeIdValue,
                                    NumAdditionalArgsValue,
                                    IncomingValueNameValue};
    return std::move(Args);
  }

  // get arguments from printValue
  std::vector<llvm::Value *> getPrintValueArgs(const char Tag,
                                               llvm::Value *Parameter,
                                               llvm::IRBuilder<> &Builder) {
    auto TagValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt8Ty(this->Module->getContext()), Tag, false);
    auto Name = Parameter->getName();
    auto NameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, Name);

    auto Type = Parameter->getType();
    auto TypeId = Type->getTypeID();
    auto TypeIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()), TypeId,
        false);

    unsigned NumAdditionalArgs = 0;
    std::vector<llvm::Value *> AdditionalArgValues;
    switch (TypeId) {
    case llvm::Type::TypeID::IntegerTyID:
    case llvm::Type::TypeID::DoubleTyID: {
      NumAdditionalArgs = 1;
      AdditionalArgValues.push_back(Parameter);
      break;
    }
    case llvm::Type::TypeID::PointerTyID: {
      NumAdditionalArgs = 1;
      if (isIntrinsicFunctionName(Name)) {
        // For instrinsic function, just use 0 as the address.
        AdditionalArgValues.push_back(llvm::ConstantInt::get(
            llvm::IntegerType::getInt32Ty(this->Module->getContext()), 0,
            false));
      } else if (llvm::isa<llvm::InlineAsm>(Parameter)) {
        // For inline asmembly, just use 1 as the address.
        AdditionalArgValues.push_back(llvm::ConstantInt::get(
            llvm::IntegerType::getInt32Ty(this->Module->getContext()), 1,
            false));
      } else {
        AdditionalArgValues.push_back(Parameter);
      }
      break;
    }
    case llvm::Type::TypeID::VectorTyID: {
      // Cast to VectorType.
      auto VectorType = llvm::cast<llvm::VectorType>(Type);
      NumAdditionalArgs = 2;
      // For vector, we have to allocate a buffer.
      auto Buffer = getOrCreateVectorStoreBuffer(
          this->VectorStoreBuffer, this->CurrentFunction, VectorType);
      // Store the vector into the buffer.
      auto TypeBytes = VectorType->getBitWidth() / 8;
      // Since the buffer can be allocated from other vector, do a bitcast.
      auto CastBuffer = Builder.CreateBitCast(Buffer, Type->getPointerTo());
      auto AlignBytes = VectorType->getScalarSizeInBits() / 8;
      Builder.CreateAlignedStore(Parameter, CastBuffer, AlignBytes);
      // The first additional arguments will be the size of the buffer.
      AdditionalArgValues.push_back(llvm::ConstantInt::get(
          llvm::IntegerType::getInt32Ty(this->Module->getContext()), TypeBytes,
          false));
      // The second additional argument will be the buffer.
      AdditionalArgValues.push_back(Buffer);
      break;
    }
    default: {
      NumAdditionalArgs = 0;
      break;
    }
    }

    assert(NumAdditionalArgs == AdditionalArgValues.size() &&
           "The number of additional arguments doesn't match.");

    auto NumAdditionalArgsValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        NumAdditionalArgs, false);

    std::vector<llvm::Value *> Args{TagValue, NameValue, TypeIdValue,
                                    NumAdditionalArgsValue};
    for (auto AdditionalArgValue : AdditionalArgValues) {
      Args.push_back(AdditionalArgValue);
    }

    return std::move(Args);
  }
};

void TracePass::findReachableFunctions() {
  assert(TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
         "TraceReachableFunctionOnly should be specified.");
  assert(TraceReachableFunctionOnly.getValue() &&
         "TraceReachableFunctionOnly should be specified as true.");
  std::list<llvm::Function *> Queue;
  for (const auto &FunctionName : this->tracedFunctionNames) {
    auto Func = this->Module->getFunction(FunctionName);
    assert(Func != nullptr && "Failed to look up function in the module.");
    Queue.push_back(Func);
  }
  while (!Queue.empty()) {
    auto Func = Queue.front();
    Queue.pop_front();
    if (this->reachableFunctions.find(Func) != this->reachableFunctions.end()) {
      continue;
    }
    this->reachableFunctions.insert(Func);
    for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
         ++BBIter) {
      for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
           InstIter != InstEnd; ++InstIter) {
        auto Inst = &*InstIter;
        if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
          if (CallInst->isInlineAsm()) {
            continue;
          }
          auto CalleeFunc = CallInst->getCalledFunction();
          if (CalleeFunc == nullptr) {
            llvm::errs() << "Trace reachable functions only specified, but "
                            "found indirect call in function "
                         << Func->getName() << '\n';
            llvm_unreachable("Trace reachable functions found indirect call.");
          }
          if (CalleeFunc->isIntrinsic()) {
            continue;
          }
          if (CalleeFunc->isDeclaration()) {
            // This is only declaration, we take this as external calls.
            continue;
          }
          // This is a reachable function with a body.
          Queue.push_back(CalleeFunc);
        }
      }
    }
  }
  DEBUG(llvm::errs()
        << "==================== Reachable Functions ==================\n");
  for (auto Func : this->reachableFunctions) {
    DEBUG(llvm::errs() << Func->getName() << '\n');
  }
  DEBUG(llvm::errs()
        << "==================== Reachable Functions ==================\n");
}

} // namespace

#undef DEBUG_TYPE

char TracePass::ID = 0;
static llvm::RegisterPass<TracePass> X("trace-pass", "Trace pass", false,
                                       false);
