#include "Utils.h"

#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Path.h"

#include "google/protobuf/util/json_util.h"

#include <sstream>

#define DEBUG_TYPE "GemForgeUtils"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

std::unordered_map<const llvm::Function *, std::string>
    Utils::MemorizedDemangledFunctionNames;

std::unordered_map<const llvm::Instruction *, const llvm::Instruction *>
    Utils::MemorizedStaticNextNonPHIInstMap;

InstructionUIDMap Utils::InstUIDMap;

const std::string &Utils::getDemangledFunctionName(const llvm::Function *Func) {
  auto MemorizedIter = Utils::MemorizedDemangledFunctionNames.find(Func);
  if (MemorizedIter == Utils::MemorizedDemangledFunctionNames.end()) {

    std::string MangledName = Func->getName();
    auto DemangledName = llvm::demangle(MangledName);
    LLVM_DEBUG(llvm::errs() << "Demangled " << MangledName << " into "
                            << DemangledName << '\n');
    std::string DemangledNameStr(DemangledName);
    auto Pos = DemangledNameStr.find('(');
    if (Pos != std::string::npos) {
      DemangledNameStr = DemangledNameStr.substr(0, Pos);
    }
    MemorizedIter =
        Utils::MemorizedDemangledFunctionNames.emplace(Func, DemangledNameStr)
            .first;
  }

  return MemorizedIter->second;
}

std::string Utils::formatLLVMFunc(const llvm::Function *Func) {
  return Utils::getInstUIDMap().getFuncUID(Func);
}

std::string Utils::formatLLVMValue(const llvm::Value *Value) {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    return Utils::formatLLVMInst(Inst);
  } else {
    // This part is the same.
    return Utils::formatLLVMValueWithoutFunc(Value);
  }
}

std::string Utils::formatLLVMValueWithoutFunc(const llvm::Value *Value) {
  if (auto Inst = llvm::dyn_cast<llvm::Instruction>(Value)) {
    return Utils::formatLLVMInstWithoutFunc(Inst);
  } else if (auto Const = llvm::dyn_cast<llvm::ConstantData>(Value)) {
    std::stringstream ss;
    ss << "Const(";
    if (auto ConstInt = llvm::dyn_cast<llvm::ConstantInt>(Const)) {
      ss << ConstInt->getValue().toString(10 /* Radix */, false /* signed */);
    }
    ss << ')';
    return ss.str();
  } else {
    return Value->getName();
  }
}

const llvm::Instruction *
Utils::getStaticNextNonPHIInst(const llvm::Instruction *Inst) {

  if (MemorizedStaticNextNonPHIInstMap.count(Inst) == 0) {
    const llvm::Instruction *StaticNextNonPHIInst = nullptr;
    const auto BB = Inst->getParent();
    if (Inst->isTerminator()) {
      const auto Func = BB->getParent();
      const llvm::BasicBlock *NextBB = nullptr;
      for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
           ++BBIter) {
        if ((&*BBIter) == BB) {
          ++BBIter;
          if (BBIter != BBEnd) {
            NextBB = &*BBIter;
          }
          break;
        }
      }
      if (NextBB != nullptr) {
        // Use the first non-phi instructions.
        StaticNextNonPHIInst = NextBB->getFirstNonPHI();
      }
    } else {
      for (auto InstIter = BB->begin(), InstEnd = BB->end();
           InstIter != InstEnd; ++InstIter) {
        if ((&*InstIter) == Inst) {
          ++InstIter;
          assert(InstIter != InstEnd && "Non-Terminator at the end of BB.");
          StaticNextNonPHIInst = &*InstIter;
        }
      }
      assert(StaticNextNonPHIInst != nullptr &&
             "Failed to find the inst in the BB.");
    }
    MemorizedStaticNextNonPHIInstMap.emplace(Inst, StaticNextNonPHIInst);
  }

  return MemorizedStaticNextNonPHIInstMap.at(Inst);
}

bool Utils::isAsmBranchInst(const llvm::Instruction *Inst) {
  switch (Inst->getOpcode()) {
  case llvm::Instruction::Call:
  case llvm::Instruction::Invoke: {
    const llvm::Value *CalleeValue = Utils::getCalledValue(Inst);
    if (llvm::isa<llvm::InlineAsm>(CalleeValue)) {
      return false;
    }
    if (auto Function = llvm::dyn_cast<llvm::Function>(CalleeValue)) {
      // Direct function call. Check if we have the definition.
      if (Function->isIntrinsic()) {
        return false;
      } else if (Function->isDeclaration()) {
        // Untraced function.
        return false;
      } else {
        return true;
      }
    } else {
      // Indirect function call.
      return true;
    }
  }
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return true;
  }
  case llvm::Instruction::Br: {
    auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(Inst);
    if (BranchInst->isConditional()) {
      return true;
    }
    /**
     * For unconditional branch, we check the static next instruction.
     * If it is the same as the successor, then we assume this branch
     * will be removed in the asm.
     */
    auto StaticNextNonPHIInst = Utils::getStaticNextNonPHIInst(Inst);
    if (StaticNextNonPHIInst == nullptr) {
      return true;
    }
    if (BranchInst->getSuccessor(0) != StaticNextNonPHIInst->getParent()) {
      return true;
    }
    return false;
  }
  default: { return false; }
  }
}

bool Utils::isAsmConditionalBranchInst(const llvm::Instruction *Inst) {
  assert(Utils::isAsmBranchInst(Inst) &&
         "isAsmConditionalBranchInst should only be called on AsmBranchInst.");
  switch (Inst->getOpcode()) {
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Invoke:
  case llvm::Instruction::Call:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return false;
  }
  case llvm::Instruction::Br: {
    auto BranchInst = llvm::dyn_cast<llvm::BranchInst>(Inst);
    return BranchInst->isConditional();
  }
  default: { return false; }
  }
}

bool Utils::isAsmIndirectBranchInst(const llvm::Instruction *Inst) {
  assert(Utils::isAsmBranchInst(Inst) &&
         "isAsmIndirectBranchInst should only be called on AsmBranchInst.");
  switch (Inst->getOpcode()) {
  case llvm::Instruction::Br: {
    return false;
  }
  case llvm::Instruction::IndirectBr:
  case llvm::Instruction::Ret:
  case llvm::Instruction::Switch: {
    return true;
  }
  case llvm::Instruction::Invoke:
  case llvm::Instruction::Call: {
    auto CalleeValue = Utils::getCalledValue(Inst);
    return !llvm::isa<llvm::Function>(CalleeValue);
  }

  default: { return false; }
  }
}

void Utils::dumpProtobufMessageToJson(
    const ::google::protobuf::Message &Message, const ::std::string &FileName) {
  std::ofstream InfoTextFStream(FileName);
  assert(InfoTextFStream.is_open() &&
         "Failed to open the output info text file.");
  std::string InfoJsonString;
  ::google::protobuf::util::JsonPrintOptions JsonPrintOptions;
  JsonPrintOptions.add_whitespace = true;
  JsonPrintOptions.always_print_primitive_fields = true;
  ::google::protobuf::util::MessageToJsonString(Message, &InfoJsonString,
                                                JsonPrintOptions);
  InfoTextFStream << InfoJsonString;
  InfoTextFStream.close();
}

std::unordered_set<llvm::Function *>
Utils::decodeFunctions(std::string FuncNames, llvm::Module *Module) {
  std::unordered_set<std::string> UnmatchedNames;
  std::unordered_set<llvm::Function *> MatchedFunctions;

  size_t Prev = 0;
  for (size_t Curr = 0; Curr <= FuncNames.size(); ++Curr) {
    if (FuncNames[Curr] == FuncNameSeparator || Curr == FuncNames.size()) {
      if (Prev < Curr) {
        auto Name = FuncNames.substr(Prev, Curr - Prev);

        // Make sure that either this name is in the function, or there
        // is a demangled name for this name.
        auto Function = Module->getFunction(Name);
        if (Function != nullptr) {
          // We found the function directly.
          // After this point we use mangled name everywhere.
          MatchedFunctions.insert(Function);
          LLVM_DEBUG(llvm::errs()
                     << "Add function " << Function->getName() << '\n');
        } else {
          UnmatchedNames.insert(Name);
        }
      }
      Prev = Curr + 1;
    }
  }

  // Try to demangle the names in the module, to find a match for unmatched
  // names.
  for (auto FuncIter = Module->begin(), FuncEnd = Module->end();
       FuncIter != FuncEnd; ++FuncIter) {
    if (FuncIter->isDeclaration()) {
      // Ignore declaration.
      continue;
    }
    auto DemangledName = getDemangledFunctionName(&*FuncIter);
    auto UnmatchedNameIter = UnmatchedNames.find(DemangledName);
    if (UnmatchedNameIter != UnmatchedNames.end()) {
      // We found a match.
      // We always use mangled name hereafter for simplicity.
      MatchedFunctions.insert(&*FuncIter);
      LLVM_DEBUG(llvm::errs()
                 << "Add traced function " << DemangledName << '\n');
      UnmatchedNames.erase(UnmatchedNameIter);
    }
  }

  for (const auto &Name : UnmatchedNames) {
    llvm::errs() << "Unable to find match for trace function " << Name << '\n';
  }
  assert(UnmatchedNames.empty() &&
         "Unabled to find match for some trace function.");
  return MatchedFunctions;
}

#undef DEBUG_TYPE