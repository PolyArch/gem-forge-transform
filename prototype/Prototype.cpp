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

#define DEBUG_TYPE "PrototypePass"
namespace {
class Prototype : public llvm::FunctionPass {
 public:
  static char ID;
  Prototype() : llvm::FunctionPass(ID) {}
  void getAnalysisUsage(llvm::AnalysisUsage& Info) const override {
    // We require the loop information.
    Info.addRequired<llvm::LoopInfoWrapperPass>();
    Info.addPreserved<llvm::LoopInfoWrapperPass>();
    Info.addRequiredID(llvm::InstructionNamerID);
    Info.addPreservedID(llvm::InstructionNamerID);
  }
  bool doInitialization(llvm::Module& Module) override {
    this->Module = &Module;
    Workload = "foo";
    DEBUG(llvm::errs() << "Initialize Prototype with workload: " << Workload
                       << '\n');

    // Register the external log functions.
    registerPrintFunctions(Module);

    return true;
  }
  bool runOnFunction(llvm::Function& Function) override {
    auto FunctionName = Function.getName().str();
    DEBUG(llvm::errs() << "FunctionName: " << FunctionName << '\n');

    // For now run on any functions we have found.
    // if (FunctionName != Workload) {
    //   return false;
    // }
    DEBUG(llvm::errs() << "Found workload: " << FunctionName << '\n');

    for (auto BBIter = Function.begin(), BBEnd = Function.end();
         BBIter != BBEnd; ++BBIter) {
      DEBUG(llvm::errs() << "Found basic block: " << BBIter->getName() << '\n');
      runOnBasicBlock(*BBIter);
    }

    // We always modify the function.
    return true;
  }

 private:
  std::string Workload;
  llvm::Module* Module;

  /* Print functions. */
  llvm::Value* PrintInstFunc;
  llvm::Value* PrintValueFunc;

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
    PrintInstFunc = Module.getOrInsertFunction("printInst", PrintInstTy);

    std::vector<llvm::Type*> PrintValueArgs{
        Int8PtrTy,
        Int8PtrTy,  // char* Name,
        Int8PtrTy, Int32Ty, Int32Ty,
    };
    auto PrintValueTy = llvm::FunctionType::get(VoidTy, PrintValueArgs, true);
    PrintValueFunc = Module.getOrInsertFunction("printValue", PrintValueTy);
  }

  std::map<std::string, llvm::Constant*> GlobalStrings;

  llvm::Constant* getOrCreateStringLiteral(const std::string& Str) {
    if (GlobalStrings.find(Str) == GlobalStrings.end()) {
      // Create the constant array.
      auto Array = llvm::ConstantDataArray::getString(
          this->Module->getContext(), Str, true);
      // Get the array type.
      auto ElementTy = llvm::IntegerType::get(this->Module->getContext(), 8);
      auto ArrayTy = llvm::ArrayType::get(ElementTy, Str.size() + 1);
      // Register a global variable.
      auto GlobalVariableStr = new llvm::GlobalVariable(
          *(this->Module), ArrayTy, true, llvm::GlobalValue::PrivateLinkage,
          Array, ".str");
      GlobalVariableStr->setAlignment(1);
      // Get the address of the first element in the string.
      // Notice that the global variable %.str is also a pointer, that's why we
      // need two indexes. See great tutorial:
      // https://llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction
      //   auto ConstantZero =
      //   llvm::ConstantInt::get(this->Module->getContext(),
      //  llvm::APInt(32, 0));
      auto ConstantZero = llvm::ConstantInt::get(
          llvm::IntegerType::getInt32Ty(this->Module->getContext()), 0, false);
      std::vector<llvm::Constant*> Indexes;
      Indexes.push_back(ConstantZero);
      Indexes.push_back(ConstantZero);
      GlobalStrings[Str] = llvm::ConstantExpr::getGetElementPtr(
          ArrayTy, GlobalVariableStr, Indexes);
    }
    return GlobalStrings[Str];
  }

  bool runOnBasicBlock(llvm::BasicBlock& BB) {
    llvm::BasicBlock::iterator NextInstIter;
    
    auto FunctionNameValue =
        getOrCreateStringLiteral(BB.getParent()->getName());
    auto BBNameValue = getOrCreateStringLiteral(BB.getName());

    unsigned InstId = 0;
    for (auto InstIter = BB.begin(); InstIter != BB.end();
         InstIter = NextInstIter, InstId++) {
      NextInstIter = InstIter;
      NextInstIter++;

      std::string OpCodeName = InstIter->getOpcodeName();
      DEBUG(llvm::errs() << "Found instructions: " << OpCodeName << '\n');
      // if (OpCodeName == "alloca") {
      //   // Ignore all the alloca instructions.
      //   continue;
      // }
      llvm::Instruction* Inst = &*InstIter;
      if (auto* PHI = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        // Special case for phi node as they must be the first
        // inst of the basic block.
        assert(InstId == 0 && "Phi node must be the first inst in BB.");
        tracePhiInst(PHI, FunctionNameValue, BBNameValue, InstId);
        continue;
      }

      traceNonPhiInst(Inst, FunctionNameValue, BBNameValue, InstId);
    }
    return true;
  }

  // Phi node has to be handled specially as it must be the first
  // instruction of a basic block.
  void tracePhiInst(llvm::PHINode* Inst, llvm::Constant* FunctionNameValue,
                    llvm::Constant* BBNameValue, unsigned InstId) {
    std::vector<llvm::Value*> PrintInstArgs =
        getPrintInstArgs(Inst, FunctionNameValue, BBNameValue, InstId);
    // Call printInst. After the instruction.
    DEBUG(llvm::errs() << "Insert printInst for phi node.\n");
    llvm::IRBuilder<> Builder(Inst->getNextNode());
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
      auto PrintValueArgs = getPrintValueArgs("r", Inst);
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
    DEBUG(llvm::errs() << "Insert printInst\n");
    llvm::IRBuilder<> Builder(Inst);
    Builder.CreateCall(this->PrintInstFunc, PrintInstArgs);

    // Call printValue for each parameter before the instruction.
    for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
         OperandId < NumOperands; OperandId++) {
      DEBUG(llvm::errs() << "Insert printValue\n");
      auto PrintValueArgs = getPrintValueArgs("p", Inst->getOperand(OperandId));
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }

    // Call printResult after the instruction (if it has a result).
    if (Inst->getName() != "") {
      DEBUG(llvm::errs() << "Insert printResult\n");
      Builder.SetInsertPoint(Inst->getNextNode());
      auto PrintValueArgs = getPrintValueArgs("r", Inst);
      Builder.CreateCall(this->PrintValueFunc, PrintValueArgs);
    }
  }

  std::vector<llvm::Value*> getPrintInstArgs(llvm::Instruction* Inst,
                                             llvm::Constant* FunctionNameValue,
                                             llvm::Constant* BBNameValue,
                                             unsigned InstId) {
    std::string OpCodeName = Inst->getOpcodeName();

    // Create the string literal.
    auto OpCodeNameValue = getOrCreateStringLiteral(OpCodeName);

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
    auto TagValue = getOrCreateStringLiteral("p");
    auto Name = IncomingBlock->getName();
    auto NameValue = getOrCreateStringLiteral(Name);

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
    auto TypeNameValue = getOrCreateStringLiteral(TypeName);

    unsigned NumAdditionalArgs = 1;
    auto NumAdditionalArgsValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        NumAdditionalArgs, false);

    auto IncomingValueName = IncomingValue->getName();
    auto IncomingValueNameValue = getOrCreateStringLiteral(IncomingValueName);

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
                                              llvm::Value* Parameter) {
    auto TagValue = getOrCreateStringLiteral(Tag);
    auto Name = Parameter->getName();
    auto NameValue = getOrCreateStringLiteral(Name);

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
    auto TypeNameValue = getOrCreateStringLiteral(TypeName);

    unsigned NumAdditionalArgs = 0;
    switch (TypeId) {
      case llvm::Type::TypeID::IntegerTyID: {
        NumAdditionalArgs = 1;
        break;
      }
      case llvm::Type::TypeID::PointerTyID: {
        NumAdditionalArgs = 1;
        break;
      }
      case llvm::Type::TypeID::LabelTyID: {
        NumAdditionalArgs = 1;
        break;
      }
    }
    auto NumAdditionalArgsValue = llvm::ConstantInt::get(
        llvm::IntegerType::getInt32Ty(this->Module->getContext()),
        NumAdditionalArgs, false);

    std::vector<llvm::Value*> Args{TagValue, NameValue, TypeNameValue,
                                   TypeIdValue, NumAdditionalArgsValue};
    if (NumAdditionalArgs == 1) {
      Args.push_back(Parameter);
    }
    return std::move(Args);
  }
};
}  // namespace

#undef DEBUG_TYPE

char Prototype::ID = 0;
static llvm::RegisterPass<Prototype> X("prototype", "Prototype pass", false,
                                       false);