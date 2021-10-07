#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"

#include "LocateAccelerableFunctions.h"
#include "TransformUtils.h"
#include "trace/InstructionUIDMap.h"
#include "trace/Tracer.h"

#include <map>
#include <set>
#include <unordered_set>

enum TraceDetailLevelE {
  // Instructions with operands and results.
  Detail = 0,
  // Instructions only (no operands or results).
  InstructionOnly,
  // BBs only (By only trace FirstInst/Ret/Landingpad
  // without operands or results).
  // We need ret/landingpad in case we want to track
  // stack at run time.
  BasicBlockOnly,
};

static llvm::cl::opt<std::string> TraceFunctionNames(
    "trace-function",
    llvm::cl::desc("Trace function names. For C functions, just the name. For "
                   "C++ Functions, the signature is required. Dot separated."));

static llvm::cl::opt<std::string>
    InstUIDFileName("trace-inst-uid-file",
                    llvm::cl::desc("File name to dump the instruction uid map "
                                   "to compress the trace size."));

namespace {
// The separator has to be something else than ',()*&:[a-z][A-Z][0-9]_', which
// will appear in the C++ function signature.
const char TraceFunctionNameSeparator = '.';
} // namespace

static llvm::cl::opt<TraceDetailLevelE> TraceDetailLevel(
    "trace-detail-level", llvm::cl::desc("Choose trace detail level:"),
    llvm::cl::values(
        clEnumValN(TraceDetailLevelE::Detail, "detail",
                   "All instructions with operands/results."),
        clEnumValN(TraceDetailLevelE::InstructionOnly, "inst-only",
                   "All instructions without operands/results.."),
        clEnumValN(TraceDetailLevelE::BasicBlockOnly, "bb-only",
                   "First instruction in BB without operands/results.")));

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
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

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
    GlobalStrings[Str] = TransformUtils::insertStringLiteral(Module, Str);
  }
  return GlobalStrings.at(Str);
}

// This is a pass to instrument a function and trace it.
class TracePass : public llvm::FunctionPass {
public:
  static char ID;
  TracePass() : llvm::FunctionPass(ID) {}
  void getAnalysisUsage(llvm::AnalysisUsage &Info) const override {
    // LLVM_DEBUG(llvm::dbgs() << "We have required the analysis.\n");
    Info.addRequired<LocateAccelerableFunctions>();
    Info.addPreserved<LocateAccelerableFunctions>();
  }
  bool doInitialization(llvm::Module &Module) override {
    this->Module = &Module;
    this->DataLayout = new llvm::DataLayout(this->Module);

    LLVM_DEBUG(llvm::dbgs() << "Initialize TracePass\n");

    // Register the external log functions.
    registerPrintFunctions(Module);

    // If trace function specified, find all the reachable functions.
    // Parse the dot separated list.
    if (TraceFunctionNames.getNumOccurrences() == 1) {
      size_t Prev = 0;
      // Append a comma to eliminate a corner case.
      this->tracedFunctions =
          Utils::decodeFunctions(TraceFunctionNames.getValue(), this->Module);
      // Make sure StreamMemIntrinsic is traced, if presented.
      if (auto StreamMemsetFunc = this->Module->getFunction("stream_memset")) {
        this->tracedFunctions.insert(StreamMemsetFunc);
      }
      if (auto StreamMemcpyFunc = this->Module->getFunction("stream_memcpy")) {
        this->tracedFunctions.insert(StreamMemcpyFunc);
      }
      if (auto StreamMemmoveFunc =
              this->Module->getFunction("stream_memmove")) {
        this->tracedFunctions.insert(StreamMemmoveFunc);
      }
      if (TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
          TraceReachableFunctionOnly.getValue()) {
        this->findReachableFunctions();
      }
    }

    // Insert "main" function into the must traced set.
    this->mustTracedFunctionNames.insert("main");

    return true;
  }

  bool runOnFunction(llvm::Function &Function) override {
    auto FunctionName = Function.getName().str();
    LLVM_DEBUG(llvm::dbgs() << "Processing Function: " << FunctionName << '\n');

    if (this->mustTracedFunctionNames.count(FunctionName) == 0) {
      // This function is not in the must traced set.
      if (TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
          TraceReachableFunctionOnly.getValue()) {
        if (this->reachableFunctions.find(&Function) ==
            this->reachableFunctions.end()) {
          // This function is not reachable.
          // We safely ignore it.
          return false;
        }
      }
    }

    // Set the current function.
    this->CurrentFunction = &Function;
    // Clear the vector store buffer.
    this->VectorStoreBuffer.clear();

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      // LLVM_DEBUG(llvm::dbgs() << "Found basic block: " << BBIter->getName()
      // <<
      // '\n');
      runOnBasicBlock(*BBIter);
    }

    // Trace the function args.
    traceFuncArgs(Function);

    // We always modify the function.
    return true;
  }

  bool doFinalization(llvm::Module &Module) override {
    // Remember to serialize the InstructionUIDMap only when user specified the
    // file name.
    if (InstUIDFileName.getNumOccurrences() > 0) {
      this->InstUIDMap.serializeTo(InstUIDFileName.getValue());
      this->InstUIDMap.serializeToTxt(InstUIDFileName.getValue() + ".txt");
    }

    delete this->DataLayout;
    this->DataLayout = nullptr;
    return false;
  }

private:
  llvm::Module *Module;
  llvm::DataLayout *DataLayout;
  llvm::Function *CurrentFunction;
  std::map<unsigned int, llvm::Value *> VectorStoreBuffer;

  InstructionUIDMap InstUIDMap;

  /* Print functions. */
  llvm::FunctionCallee GetOrAllocateDataBufferFunc;
  llvm::FunctionCallee PrintInstFunc;
  llvm::FunctionCallee PrintValueFunc;
  llvm::FunctionCallee PrintInstValueFunc;
  llvm::FunctionCallee PrintFuncEnterFunc;
  llvm::FunctionCallee PrintInstEndFunc;
  llvm::FunctionCallee PrintFuncEnterEndFunc;

  // Contains all the traced functions, if specified.
  std::unordered_set<llvm::Function *> tracedFunctions;

  /**
   * Store the reachable function from tracedFunctions.
   */
  std::unordered_set<llvm::Function *> reachableFunctions;

  /**
   * Contains the must trace functions, currently only the main function.
   */
  std::unordered_set<std::string> mustTracedFunctionNames;

  llvm::Value *getOrCreateVectorStoreBuffer(llvm::VectorType *Type,
                                            llvm::IRBuilder<> &Builder);

  void findReachableFunctions();

  // Insert all the print function declaration into the module.
  void registerPrintFunctions(llvm::Module &Module) {
    auto &Context = Module.getContext();
    auto VoidTy = llvm::Type::getVoidTy(Context);
    auto Int8PtrTy = llvm::Type::getInt8PtrTy(Context);
    auto Int8Ty = llvm::Type::getInt8Ty(Context);
    auto Int32Ty = llvm::Type::getInt32Ty(Context);
    auto Int64Ty = llvm::Type::getInt64Ty(Context);

    std::vector<llvm::Type *> GetOrAllocateDataBufferArgs{
        Int64Ty, // uint64_t size
    };
    auto GetOrAllocateDataBufferTy =
        llvm::FunctionType::get(Int8PtrTy, GetOrAllocateDataBufferArgs, false);
    this->GetOrAllocateDataBufferFunc = Module.getOrInsertFunction(
        "getOrAllocateDataBuffer", GetOrAllocateDataBufferTy);

    std::vector<llvm::Type *> PrintInstArgs{
        Int8PtrTy, // char* FunctionName
        Int64Ty,   // uint64_t UID,
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

    std::vector<llvm::Type *> PrintInstValueArgs{
        Int32Ty, // unsigned NumAdditionalArgs,
    };
    auto PrintInstValueTy =
        llvm::FunctionType::get(VoidTy, PrintInstValueArgs, true);
    this->PrintInstValueFunc =
        Module.getOrInsertFunction("printInstValue", PrintInstValueTy);

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

    bool IsTraced = this->tracedFunctions.count(&Function);
    auto IsTracedValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        (IsTraced ? 1 : 0), false);

    auto PrintFuncEnterArgs = std::vector<llvm::Value *>{
        getOrCreateStringLiteral(this->GlobalStrings, this->Module,
                                 Function.getName().str()),
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
        this->GlobalStrings, this->Module, BB.getParent()->getName().str());
    // HeadInst is an instruction that must be at the first place of BB.
    // e.g. PHINode, landingpad.
    auto HeadInsts = std::vector<std::pair<llvm::Instruction *, unsigned>>();
    unsigned PosInBB = 0;

    auto IsHeadInst = [](llvm::Instruction *Inst) -> bool {
      return llvm::isa<llvm::PHINode>(Inst) ||
             llvm::isa<llvm::LandingPadInst>(Inst);
    };

    LLVM_DEBUG(llvm::dbgs() << "Inside bb " << BB.getName() << '\n');

    for (auto InstIter = BB.begin(); InstIter != BB.end();
         InstIter = NextInstIter, PosInBB++) {
      NextInstIter = InstIter;
      NextInstIter++;

      std::string OpCodeName = InstIter->getOpcodeName();
      LLVM_DEBUG(llvm::dbgs() << "Found instructions: " << OpCodeName << '\n');
      llvm::Instruction *Inst = &*InstIter;

      bool ShouldTrace = true;
      if (PosInBB > 0 &&
          TraceDetailLevel == TraceDetailLevelE::BasicBlockOnly) {
        // We ingore NonFirst instructions unless this is a landingpad/ret.
        if (Inst->getOpcode() != llvm::Instruction::Ret &&
            Inst->getOpcode() != llvm::Instruction::LandingPad) {
          ShouldTrace = false;
        }
      }

      if (ShouldTrace) {
        if (IsHeadInst(Inst)) {
          // Special case for head instructions as they must be grouped at the
          // top of the basic block. Push them into a vector for later
          // processing.
          HeadInsts.emplace_back(Inst, PosInBB);
        } else {
          traceNonPhiInst(Inst, FunctionNameValue, PosInBB);
        }
      }
    }

    LLVM_DEBUG(llvm::dbgs() << "After transformation.\n");
    for (auto InstIter = BB.begin(); InstIter != BB.end(); ++InstIter) {
      std::string OpCodeName = InstIter->getOpcodeName();
      LLVM_DEBUG(llvm::dbgs() << "Found instructions: " << OpCodeName << '\n');
    }

    // Handle all the phi nodes now.
    if (!HeadInsts.empty()) {
      auto InsertBefore = &*(BB.getFirstInsertionPt());
      // Ignore landingpadinst.
      assert(!IsHeadInst(InsertBefore) &&
             "InsertBefore should not be head instruction.");
      for (auto &HeadInstPair : HeadInsts) {
        auto HeadInst = HeadInstPair.first;
        auto PosInBB = HeadInstPair.second;
        if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(HeadInst)) {
          tracePhiInst(PHIInst, FunctionNameValue, PosInBB, InsertBefore);
        } else if (llvm::isa<llvm::LandingPadInst>(HeadInst)) {
          // landingpad inst can be traced as normal instruction?
          traceNonPhiInst(HeadInst, FunctionNameValue, PosInBB, InsertBefore);
        } else {
          llvm_unreachable("Invalid head instruction.");
        }
      }
    }

    return true;
  }

  // Phi node has to be handled specially as it must be the first
  // instruction of a basic block.
  // All the trace functions will be inserted before @InsertBefore.
  void tracePhiInst(llvm::PHINode *Inst, llvm::Constant *FunctionNameValue,
                    unsigned PosInBB, llvm::Instruction *InsertBefore) {
    std::vector<llvm::Value *> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, PosInBB);
    // Call printInst. After the instruction.
    // LLVM_DEBUG(llvm::dbgs() << "Insert printInst for phi node.\n");
    llvm::IRBuilder<> Builder(InsertBefore);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    if (TraceDetailLevel == TraceDetailLevelE::Detail) {
      // Call printValue for each parameter before the instruction.
      for (unsigned IncomingValueId = 0,
                    NumIncomingValues = Inst->getNumIncomingValues();
           IncomingValueId < NumIncomingValues; IncomingValueId++) {
        // LLVM_DEBUG(llvm::dbgs() << "Insert printValue for phi node.\n");
        auto PrintValueArgs = getPrintValueArgsForPhiParameter(
            Inst->getIncomingValue(IncomingValueId),
            Inst->getIncomingBlock(IncomingValueId));
        Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
      }

      // Call printResult after the instruction (if it has a result).
      if (Inst->getName() != "") {
        // LLVM_DEBUG(llvm::dbgs() << "Insert printResult for phi node.\n");
        auto PrintValueArgs =
            getPrintValueArgs(PRINT_VALUE_TAG_RESULT, Inst, Builder);
        Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
      }
    }
    Builder.CreateCall(this->PrintInstEndFunc);
  }

  // Insert the trace call to non phi instructions.
  void traceNonPhiInst(llvm::Instruction *Inst,
                       llvm::Constant *FunctionNameValue, unsigned PosInBB,
                       llvm::Instruction *InsertBefore = nullptr) {
    std::string OpCodeName = Inst->getOpcodeName();
    assert(OpCodeName != "phi" && "traceNonPhiInst can't trace phi inst.");

    // LLVM_DEBUG(llvm::dbgs() << "Trace non-phi inst " << Inst->getName() << "
    // op
    // "
    //                    << Inst->getOpcodeName() << '\n');

    std::vector<llvm::Value *> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, PosInBB);
    // Call printInst. Before the instruction.
    llvm::IRBuilder<> Builder((InsertBefore == nullptr) ? Inst : InsertBefore);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    if (TraceDetailLevel == TraceDetailLevelE::Detail) {
      // Call printValue for each parameter before the instruction.
      for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
           OperandId < NumOperands; OperandId++) {
        auto PrintInstValueArgs = getPrintInstValueArgs(
            PRINT_VALUE_TAG_PARAMETER, Inst->getOperand(OperandId), Builder);

        Builder.CreateCall(this->PrintInstValueFunc, PrintInstValueArgs);
      }

      // Call printResult after the instruction (if it has a result).
      bool shouldTraceResult = Inst->getName() != "";
      if (shouldTraceResult) {
        if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(Inst)) {
          /**
           * Do not take the risk of tracing the result of call instruction.
           * If the callee is traced, then the pair of (printInst,
           * printInstEnd) for the call instruction is likely interleaved with
           * the the print* calls in the callee. And there is no way for us to
           * determine whether the callee is traced, so I just ignore the
           * result of call instruction. The result of the call can be filled
           * in either by the callee's traced ret instruction, or by other
           * users in the same function.
           */
          shouldTraceResult = false;
        } else if (auto InvokeInst = llvm::dyn_cast<llvm::InvokeInst>(Inst)) {
          /**
           * Similar reason as call instruction, but also due to that
           * LLVM requires a terminator for each basic block.
           * Most of the terminators does not produce values,
           * thus no need to trace the result.
           * However, the only exception is 'invoke'.
           * This should be fine for standalone mode.
           */
          shouldTraceResult = false;
        }
      }

      if (shouldTraceResult) {
        if (InsertBefore == nullptr) {
          // Set the insertion point after the instruction.
          llvm::Instruction *NextInst = Inst->getNextNode();
          if (NextInst == nullptr) {
            assert(false && "Missing tracing last inst in block.");
          } else {
            assert(NextInst != nullptr && "Null next inst.");
            Builder.SetInsertPoint(NextInst);
          }
        }

        auto PrintInstValueArgs =
            getPrintInstValueArgs(PRINT_VALUE_TAG_RESULT, Inst, Builder);
        Builder.CreateCall(this->PrintInstValueFunc, PrintInstValueArgs);
      }
    }
    // Call printInstEnd to commit.
    Builder.CreateCall(this->PrintInstEndFunc);
  }

  std::vector<llvm::Value *> getPrintInstArgs(llvm::Instruction *Inst,
                                              llvm::Constant *FunctionNameValue,
                                              unsigned PosInBB) {
    // Allocate the UID for this instruction.
    auto InstUID = this->InstUIDMap.getOrAllocateUID(Inst, PosInBB);
    auto InstUIDValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt64Ty(this->Module->getContext()), InstUID,
        false);

    std::vector<llvm::Value *> PrintInstArgs;
    PrintInstArgs.push_back(FunctionNameValue);
    PrintInstArgs.push_back(InstUIDValue);
    return PrintInstArgs;
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
    auto Name = IncomingBlock->getName().str();
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

    auto IncomingValueName = IncomingValue->getName().str();
    auto IncomingValueNameValue = getOrCreateStringLiteral(
        this->GlobalStrings, this->Module, IncomingValueName);

    std::vector<llvm::Value *> Args{TagValue, NameValue, TypeIdValue,
                                    NumAdditionalArgsValue,
                                    IncomingValueNameValue};
    return Args;
  }

  // get arguments from printValue
  std::vector<llvm::Value *> getPrintInstValueArgs(const char Tag,
                                                   llvm::Value *Parameter,
                                                   llvm::IRBuilder<> &Builder) {
    auto Name = Parameter->getName().str();
    auto Type = Parameter->getType();
    auto TypeId = Type->getTypeID();

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
    case llvm::Type::TypeID::FixedVectorTyID: {
      // Cast to VectorType.
      auto VectorType = llvm::cast<llvm::FixedVectorType>(Type);
      NumAdditionalArgs = 2;
      // For vector, we have to allocate a buffer.
      auto Buffer = this->getOrCreateVectorStoreBuffer(VectorType, Builder);
      // Store the vector into the buffer.
      auto TypeBytes = this->DataLayout->getTypeSizeInBits(Type) / 8;
      // Since the buffer can be allocated from other vector, do a bitcast.
      auto CastBuffer = Builder.CreateBitCast(Buffer, Type->getPointerTo());
      // Align with 1 byte is always safe.
      // auto AlignBytes = VectorType->getScalarSizeInBits() / 8;
      llvm::MaybeAlign AlignBytes(1);
      Builder.CreateAlignedStore(Parameter, CastBuffer, AlignBytes);
      /**
       * For some corner case, the bit width will be less than 8!
       * For example, vector<4 x i1> for boolean types.
       * In this case, we have to use datalayout for storage bytes.
       */
      auto TypeStorageBytes = this->DataLayout->getTypeStoreSize(VectorType);
      AdditionalArgValues.push_back(llvm::ConstantInt::get(
          llvm::IntegerType::getInt32Ty(this->Module->getContext()),
          TypeStorageBytes, false));
      // The second additional argument will be the buffer.
      AdditionalArgValues.push_back(Buffer);
      break;
    }
    case llvm::Type::TypeID::LabelTyID: {
      NumAdditionalArgs = 1;
      // For label, log the name.
      auto NameValue =
          getOrCreateStringLiteral(this->GlobalStrings, this->Module, Name);
      AdditionalArgValues.push_back(NameValue);
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

    std::vector<llvm::Value *> Args{NumAdditionalArgsValue};
    for (auto AdditionalArgValue : AdditionalArgValues) {
      Args.push_back(AdditionalArgValue);
    }

    return Args;
  }

  // get arguments from printValue
  std::vector<llvm::Value *> getPrintValueArgs(const char Tag,
                                               llvm::Value *Parameter,
                                               llvm::IRBuilder<> &Builder) {
    auto TagValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt8Ty(this->Module->getContext()), Tag, false);
    auto Name = Parameter->getName().str();
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
    case llvm::Type::TypeID::FixedVectorTyID: {
      // Cast to VectorType.
      auto VectorType = llvm::cast<llvm::FixedVectorType>(Type);
      NumAdditionalArgs = 2;
      // For vector, we have to allocate a buffer.
      auto Buffer = this->getOrCreateVectorStoreBuffer(VectorType, Builder);
      // Store the vector into the buffer.
      auto TypeBytes = this->DataLayout->getTypeSizeInBits(Type) / 8;
      // Since the buffer can be allocated from other vector, do a bitcast.
      auto CastBuffer = Builder.CreateBitCast(Buffer, Type->getPointerTo());
      // Align with 1 byte is always safe.
      // auto AlignBytes = VectorType->getScalarSizeInBits() / 8;
      llvm::MaybeAlign AlignBytes(1);
      Builder.CreateAlignedStore(Parameter, CastBuffer, AlignBytes);
      /**
       * For some corner case, the bit width will be less than 8!
       * For example, vector<4 x i1> for boolean types.
       * In this case, we have to use datalayout for storage bytes.
       */
      auto TypeStorageBytes = this->DataLayout->getTypeStoreSize(VectorType);
      AdditionalArgValues.push_back(llvm::ConstantInt::get(
          llvm::IntegerType::getInt32Ty(this->Module->getContext()),
          TypeStorageBytes, false));
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

    return Args;
  }
};

void TracePass::findReachableFunctions() {
  assert(TraceReachableFunctionOnly.getNumOccurrences() == 1 &&
         "TraceReachableFunctionOnly should be specified.");
  assert(TraceReachableFunctionOnly.getValue() &&
         "TraceReachableFunctionOnly should be specified as true.");
  std::list<llvm::Function *> Queue;
  for (const auto &Func : this->tracedFunctions) {
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
  LLVM_DEBUG(
      llvm::errs()
      << "==================== Reachable Functions ==================\n");
  for (auto Func : this->reachableFunctions) {
    LLVM_DEBUG(llvm::dbgs() << Func->getName() << '\n');
  }
  LLVM_DEBUG(
      llvm::errs()
      << "==================== Reachable Functions ==================\n");
}

llvm::Value *
TracePass::getOrCreateVectorStoreBuffer(llvm::VectorType *Type,
                                        llvm::IRBuilder<> &Builder) {
  auto TypeStorageBytes = this->DataLayout->getTypeStoreSize(Type);

  /**
   * The original approach is to allocate it on stack (see the commented out
   * code), but this will result in stack overflow in some deeply recursive
   * benchmarks, e.g. gcc.
   *
   * The fix so far is that we allocate it at runtime with a special
   * simple function. Of course this has higher overhead, but since tracing is
   * usually not the bottlenect, we keep it this way.
   *
   * If required, we can take a hybrid approach: if the stack size has not
   * exceeded a limit, we can try to allocate it on stack; otherwise, we call
   * runtime function.
   */

  auto SizeValue = llvm::ConstantInt::get(
      llvm::IntegerType::getInt64Ty(this->Module->getContext()),
      TypeStorageBytes, false);

  std::vector<llvm::Value *> Args{SizeValue};
  return Builder.CreateCall(this->GetOrAllocateDataBufferFunc, Args);
}

} // namespace

#undef DEBUG_TYPE

char TracePass::ID = 0;
static llvm::RegisterPass<TracePass> X("trace-pass", "Trace pass", false,
                                       false);
