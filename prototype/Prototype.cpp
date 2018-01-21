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

#define DEBUG_TYPE "PrototypePass"
namespace {

bool isIntrinsicFunctionName(const std::string& FunctionName) {
  if (FunctionName.find("llvm.") == 0) {
    // Hack: return true if the name start with llvm.
    return true;
  } else {
    return false;
  }
}

llvm::Constant* getOrCreateStringLiteral(
    std::map<std::string, llvm::Constant*>& GlobalStrings, llvm::Module* Module,
    const std::string& Str) {
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
    // Get the address of the first element in the string.
    // Notice that the global variable %.str is also a pointer, that's why we
    // need two indexes. See great tutorial:
    // https://llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction
    //   auto ConstantZero =
    //   llvm::ConstantInt::get(this->Module->getContext(),
    //  llvm::APInt(32, 0));
    auto ConstantZero = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(Module->getContext()), 0, false);
    std::vector<llvm::Constant*> Indexes;
    Indexes.push_back(ConstantZero);
    Indexes.push_back(ConstantZero);
    GlobalStrings[Str] = llvm::ConstantExpr::getGetElementPtr(
        ArrayTy, GlobalVariableStr, Indexes);
  }
  return GlobalStrings.at(Str);
}

llvm::Value* getOrCreateVectorStoreBuffer(
    std::map<unsigned int, llvm::Value*>& VectorStoreBuffer,
    llvm::Function* Function, llvm::VectorType* Type) {
  auto BitSize = Type->getBitWidth();
  if (VectorStoreBuffer.find(BitSize) == VectorStoreBuffer.end()) {
    // Insert at the beginning of the function.
    auto IP = Function->front().getFirstInsertionPt();
    llvm::IRBuilder<> Builder(IP->getParent(), IP);
    llvm::Value* Alloca = Builder.CreateAlloca(Type);
    VectorStoreBuffer[BitSize] = Alloca;
  }

  return VectorStoreBuffer.at(BitSize);
}

// A analysis pass to locate all the accelerable functions.
// A function is accelerable if:
// 1. it has no system call.
// 2. the function it calls is also acclerable.
// In short, we can trace these functions and that's why they are accelerable.
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

// This is a pass to instrument a function and trace it.
class Prototype : public llvm::FunctionPass {
 public:
  static char ID;
  Prototype() : llvm::FunctionPass(ID) {}
  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    // We require the loop information.
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addPreserved<llvm::LoopInfoWrapperPass>();
    Info.addRequiredID(llvm::InstructionNamerID);
    Info.addPreservedID(llvm::InstructionNamerID);
  }
  bool doInitialization(llvm::Module& Module) override {
    this->Module = &Module;
    Workload = "fft";
    DEBUG(llvm::errs() << "Initialize Prototype with workload: " << Workload
                       << '\n');

    // Register the external log functions.
    registerPrintFunctions(Module);

    return true;
  }
  bool runOnFunction(llvm::Function& Function) override {
    auto FunctionName = Function.getName().str();
    DEBUG(llvm::errs() << "FunctionName: " << FunctionName << '\n');

    // Check if this function is accelerable.
    bool Accelerable =
        getAnalysis<LocateAccelerableFunctions>().isAccelerable(FunctionName);
    // A special hack: do not trace function which
    // returns a value, currently we donot support return in our trace.
    if (!Function.getReturnType()->isVoidTy()) {
      Accelerable = false;
    }
    if (!Accelerable) {
      return false;
    }

    // Set the current function.
    this->CurrentFunction = &Function;
    // Clear the vector store buffer.
    this->VectorStoreBuffer.clear();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      DEBUG(llvm::errs() << "Found basic block: " << BBIter->getName() << '\n');
      runOnBasicBlock(*BBIter);
    }

    // Trace the function args.
    traceFuncArgs(Function);

    // We always modify the function.
    return true;
  }

 private:
  std::string Workload;
  llvm::Module* Module;
  llvm::Function* CurrentFunction;
  std::map<unsigned int, llvm::Value*> VectorStoreBuffer;

  /* Print functions. */
  llvm::Value* PrintInstFunc;
  llvm::Value* PrintValueFunc;
  llvm::Value* PrintFuncEnterFunc;

  // Insert all the print function declaration into the module.
  void registerPrintFunctions(llvm::Module& Module) {
    auto& Context = Module.getContext();
    auto VoidTy = llvm::Type::getVoidTy(Context);
    auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
    auto Int32Ty = llvm::Type::getInt32Ty(Context);

    std::vector<llvm::Type*> PrintInstArgs{
        Int8PtrTy,  // char* FunctionName
        Int8PtrTy,  // char* BBName,
        Int32Ty,    // unsigned Id,
        Int8PtrTy,  // char* OpCodeName,
    };
    auto PrintInstTy = llvm::FunctionType::get(VoidTy, PrintInstArgs, false);
    this->PrintInstFunc = Module.getOrInsertFunction("printInst", PrintInstTy);

    std::vector<llvm::Type*> PrintValueArgs{
        Int8PtrTy,
        Int8PtrTy,  // char* Name,
        Int8PtrTy, Int32Ty, Int32Ty,
    };
    auto PrintValueTy = llvm::FunctionType::get(VoidTy, PrintValueArgs, true);
    this->PrintValueFunc =
        Module.getOrInsertFunction("printValue", PrintValueTy);

    std::vector<llvm::Type*> PrintFuncEnterArgs{
        Int8PtrTy,
    };
    auto PrintFuncEnterTy =
        llvm::FunctionType::get(VoidTy, PrintFuncEnterArgs, false);
    this->PrintFuncEnterFunc =
        Module.getOrInsertFunction("printFuncEnter", PrintFuncEnterTy);
  }

  std::map<std::string, llvm::Constant*> GlobalStrings;

  bool traceFuncArgs(llvm::Function& Function) {
    auto& EntryBB = Function.getEntryBlock();
    llvm::IRBuilder<> Builder(&*EntryBB.begin());
    auto PrintFuncEnterArgs =
        std::vector<llvm::Value*>{getOrCreateStringLiteral(
            this->GlobalStrings, this->Module, Function.getName())};
    Builder.CreateCall(this->PrintFuncEnterFunc, PrintFuncEnterArgs);
    for (auto ArgsIter = Function.arg_begin(), ArgsEnd = Function.arg_end();
         ArgsIter != ArgsEnd; ArgsIter++) {
      auto PrintValueArgs = getPrintValueArgs("p", &*ArgsIter, Builder);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }
    return true;
  }

  bool runOnBasicBlock(llvm::BasicBlock& BB) {
    llvm::BasicBlock::iterator NextInstIter;

    auto FunctionNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, BB.getParent()->getName());
    auto BBNameValue = getOrCreateStringLiteral(this->GlobalStrings,
                                                this->Module, BB.getName());
    auto PHIInsts = std::vector<std::pair<llvm::PHINode*, unsigned>>();
    unsigned InstId = 0;
    for (auto InstIter = BB.begin(); InstIter != BB.end();
         InstIter = NextInstIter, InstId++) {
      NextInstIter = InstIter;
      NextInstIter++;

      std::string OpCodeName = InstIter->getOpcodeName();
      DEBUG(llvm::errs() << "Found instructions: " << OpCodeName << '\n');
      llvm::Instruction* Inst = &*InstIter;

      if (auto* PHI = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        // Special case for phi node as they must be grouped at the top
        // of the basic block.
        // Push them into a vector for later processing.
        PHIInsts.emplace_back(PHI, InstId);
        continue;
      }

      traceNonPhiInst(Inst, FunctionNameValue, BBNameValue, InstId);
    }

    // Handle all the phi nodes now.
    if (!PHIInsts.empty()) {
      auto InsertBefore = PHIInsts[PHIInsts.size() - 1].first->getNextNode();
      for (auto& PHIInstPair : PHIInsts) {
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
  void tracePhiInst(llvm::PHINode* Inst, llvm::Constant* FunctionNameValue,
                    llvm::Constant* BBNameValue, unsigned InstId,
                    llvm::Instruction* InsertBefore) {
    std::vector<llvm::Value*> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, BBNameValue, InstId);
    // Call printInst. After the instruction.
    DEBUG(llvm::errs() << "Insert printInst for phi node.\n");
    llvm::IRBuilder<> Builder(InsertBefore);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    // Call printValue for each parameter before the instruction.
    for (unsigned IncomingValueId = 0,
                  NumIncomingValues = Inst->getNumIncomingValues();
         IncomingValueId < NumIncomingValues; IncomingValueId++) {
      DEBUG(llvm::errs() << "Insert printValue for phi node.\n");
      auto PrintValueArgs = getPrintValueArgsForPhiParameter(
          Inst->getIncomingValue(IncomingValueId),
          Inst->getIncomingBlock(IncomingValueId));
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }

    // Call printResult after the instruction (if it has a result).
    if (Inst->getName() != "") {
      DEBUG(llvm::errs() << "Insert printResult for phi node.\n");
      auto PrintValueArgs = getPrintValueArgs("r", Inst, Builder);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }
  }

  // Insert the trace call to non phi instructions.
  void traceNonPhiInst(llvm::Instruction* Inst,
                       llvm::Constant* FunctionNameValue,
                       llvm::Constant* BBNameValue, unsigned InstId) {
    std::string OpCodeName = Inst->getOpcodeName();
    DEBUG(llvm::errs() << "traceNonPhiInst: " << OpCodeName << '\n');
    assert(OpCodeName != "phi" && "traceNonPhiInst can't trace phi inst.");

    std::vector<llvm::Value*> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, BBNameValue, InstId);
    // Call printInst. Before the instruction.
    llvm::IRBuilder<> Builder(Inst);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    // Call printValue for each parameter before the instruction.
    for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
         OperandId < NumOperands; OperandId++) {
      auto PrintValueArgs =
          getPrintValueArgs("p", Inst->getOperand(OperandId), Builder);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }

    // Call printResult after the instruction (if it has a result).
    if (Inst->getName() != "") {
      Builder.SetInsertPoint(Inst->getNextNode());
      auto PrintValueArgs = getPrintValueArgs("r", Inst, Builder);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }
  }

  std::vector<llvm::Value*> getPrintInstArgs(llvm::Instruction* Inst,
                                             llvm::Constant* FunctionNameValue,
                                             llvm::Constant* BBNameValue,
                                             unsigned InstId) {
    std::string OpCodeName = Inst->getOpcodeName();

    // Create the string literal.
    auto OpCodeNameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, OpCodeName);

    auto InstIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()), InstId,
        false);

    std::vector<llvm::Value*> PrintInstArgs;
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
  std::vector<llvm::Value*> getPrintValueArgsForPhiParameter(
      llvm::Value* IncomingValue, llvm::BasicBlock* IncomingBlock) {
    auto TagValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, "p");
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

    std::string TypeName;
    {
      llvm::raw_string_ostream Stream(TypeName);
      Type->print(Stream);
    }
    auto TypeNameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, TypeName);

    unsigned NumAdditionalArgs = 1;
    auto NumAdditionalArgsValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        NumAdditionalArgs, false);

    auto IncomingValueName = IncomingValue->getName();
    auto IncomingValueNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, IncomingValueName);

    std::vector<llvm::Value*> Args{TagValue,
                                   NameValue,
                                   TypeNameValue,
                                   TypeIdValue,
                                   NumAdditionalArgsValue,
                                   IncomingValueNameValue};
    return std::move(Args);
  }

  // get arguments from printValue
  std::vector<llvm::Value*> getPrintValueArgs(const std::string& Tag,
                                              llvm::Value* Parameter,
                                              llvm::IRBuilder<>& Builder) {
    auto TagValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, Tag);
    auto Name = Parameter->getName();
    auto NameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, Name);

    auto Type = Parameter->getType();
    auto TypeId = Type->getTypeID();
    auto TypeIdValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()), TypeId,
        false);

    std::string TypeName;
    {
      llvm::raw_string_ostream Stream(TypeName);
      Type->print(Stream);
    }
    auto TypeNameValue =
        getOrCreateStringLiteral(this->GlobalStrings, this->Module, TypeName);

    unsigned NumAdditionalArgs = 0;
    std::vector<llvm::Value*> AdditionalArgValues;
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
        } else {
          AdditionalArgValues.push_back(Parameter);
        }
        break;
      }
      case llvm::Type::TypeID::VectorTyID: {
        NumAdditionalArgs = 2;
        // For vector, we have to allocate a buffer.
        auto Buffer = getOrCreateVectorStoreBuffer(
            this->VectorStoreBuffer, this->CurrentFunction,
            llvm::cast<llvm::VectorType>(Type));
        // Store the vector into the buffer.
        auto TypeBytes = Type->getScalarSizeInBits() / 8;
        // Since the buffer can be allocated from other vector, do a bitcast.
        auto CastBuffer = Builder.CreateBitCast(Buffer, Type->getPointerTo());
        Builder.CreateAlignedStore(Parameter, CastBuffer, TypeBytes);
        // The first additional arguments will be the size of the buffer.
        AdditionalArgValues.push_back(llvm::ConstantInt::get(
            llvm::IntegerType::getInt32Ty(this->Module->getContext()),
            TypeBytes, false));
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

    std::vector<llvm::Value*> Args{TagValue, NameValue, TypeNameValue,
                                   TypeIdValue, NumAdditionalArgsValue};
    for (auto AdditionalArgValue : AdditionalArgValues) {
      Args.push_back(AdditionalArgValue);
    }

    return std::move(Args);
  }
};

class ReplayTrace : public llvm::FunctionPass {
 public:
  static char ID;
  ReplayTrace() : llvm::FunctionPass(ID) {}

  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addPreserved<llvm::LoopInfoWrapperPass>();
    Info.addRequiredID(llvm::InstructionNamerID);
    Info.addPreservedID(llvm::InstructionNamerID);
  }

  bool doInitialization(llvm::Module& Module) override {
    this->Module = &Module;
    Workload = "fft";
    DEBUG(llvm::errs() << "Initialize ReplaceTrace with workload: " << Workload
                       << '\n');

    // Register the external ioctl function.
    registerFunction(Module);

    return true;
  }

  bool runOnFunction(llvm::Function& Function) override {
    auto FunctionName = Function.getName().str();
    DEBUG(llvm::errs() << "FunctionName: " << FunctionName << '\n');

    // Check if this function is accelerable.
    bool Accelerable =
        getAnalysis<LocateAccelerableFunctions>().isAccelerable(FunctionName);
    // A special hack: do not trace function which
    // returns a value, currently we donot support return in our trace.
    if (!Function.getReturnType()->isVoidTy()) {
      Accelerable = false;
    }
    if (!Accelerable) {
      return false;
    }

    DEBUG(llvm::errs() << "Found workload: " << FunctionName << '\n');

    // Change the function body to call ioctl
    // to replay the trace.
    // First simply remove all BBs.
    Function.deleteBody();

    // Add another entry block with a simple ioctl call.
    auto NewBB = llvm::BasicBlock::Create(this->Module->getContext(), "entry",
                                          &Function);
    llvm::IRBuilder<> Builder(NewBB);

    for (auto ArgIter = Function.arg_begin(), ArgEnd = Function.arg_end();
         ArgIter != ArgEnd; ArgIter++) {
      if (ArgIter->getType()->getTypeID() != llvm::Type::TypeID::PointerTyID) {
        continue;
      }
      // Map all pointer
      auto ArgName = ArgIter->getName();
      auto ArgNameValue =
          getOrCreateStringLiteral(this->GlobalStrings, this->Module, ArgName);
      auto CastAddrValue = Builder.CreateCast(
          llvm::Instruction::CastOps::BitCast, &*ArgIter,
          llvm::Type::getInt8PtrTy(this->Module->getContext()));
      std::vector<llvm::Value*> MapVirtualAddrArgs{
          ArgNameValue,
          CastAddrValue,
      };
      Builder.CreateCall(this->MapVirtualAddrFunc, MapVirtualAddrArgs);
    }

    // Insert replay call.
    auto TraceNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, Function.getName());
    std::vector<llvm::Value*> ReplayArgs{
        TraceNameValue,
    };
    Builder.CreateCall(this->ReplayFunc, ReplayArgs);

    // Remember to return void.
    Builder.CreateRet(nullptr);

    // We always modify the function.
    return true;
  }

 private:
  std::string Workload;
  llvm::Module* Module;

  llvm::Value* MapVirtualAddrFunc;
  llvm::Value* ReplayFunc;

  std::map<std::string, llvm::Constant*> GlobalStrings;

  // Insert all the print function declaration into the module.
  void registerFunction(llvm::Module& Module) {
    auto& Context = Module.getContext();
    auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
    auto VoidTy = llvm::Type::getVoidTy(Context);
    auto Int32Ty = llvm::Type::getInt32Ty(Context);
    auto Int64Ty = llvm::Type::getInt64Ty(Context);

    std::vector<llvm::Type*> MapVirtualAddrArgs{
        Int8PtrTy,
        Int8PtrTy,
    };
    auto MapVirtualAddrTy =
        llvm::FunctionType::get(VoidTy, MapVirtualAddrArgs, false);
    this->MapVirtualAddrFunc =
        Module.getOrInsertFunction("mapVirtualAddr", MapVirtualAddrTy);

    std::vector<llvm::Type*> ReplayArgs{
        Int8PtrTy,
    };
    auto ReplayTy = llvm::FunctionType::get(VoidTy, ReplayArgs, false);
    this->ReplayFunc = Module.getOrInsertFunction("replay", ReplayTy);
  }
};

}  // namespace

#undef DEBUG_TYPE

char LocateAccelerableFunctions::ID = 0;
static llvm::RegisterPass<LocateAccelerableFunctions> L(
    "locate-acclerable-functions", "Locate accelerable functions", false, true);

char Prototype::ID = 0;
static llvm::RegisterPass<Prototype> X("prototype", "Prototype pass", false,
                                       false);
char ReplayTrace::ID = 0;
static llvm::RegisterPass<ReplayTrace> R("replay", "replay the llvm trace",
                                         false, false);