#include "trace/InstructionUIDMap.h"
#include "Utils.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

#include <fstream>

InstructionUIDMap::InstructionUID
InstructionUIDMap::getOrAllocateUID(llvm::Instruction *Inst, int PosInBB) {

  if (this->InstUIDMap.count(Inst) == 0) {
    this->allocateWholeFunction(Inst->getFunction());
  }

  auto UID = this->InstUIDMap.at(Inst);
  return UID;
}

void InstructionUIDMap::allocateWholeFunction(llvm::Function *Func) {
  this->allocateFunctionUID(Func);
  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    auto PosInBB = 0;
    for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
         InstIter != InstEnd; ++InstIter, ++PosInBB) {
      auto Inst = &*InstIter;

      // Return value of str is rvalue, will be moved.
      auto UID = this->AvailableUID;
      this->AvailableUID += InstructionUIDMap::InstSize;

      auto &UIDInstDescriptorMap = *(this->UIDMap.mutable_inst_map());
      UIDInstDescriptorMap.operator[](UID) = LLVM::TDG::InstructionDescriptor();

      auto &InstDescriptor = UIDInstDescriptorMap[UID];
      InstDescriptor.set_op(std::string(Inst->getOpcodeName()));
      InstDescriptor.set_func(Inst->getFunction()->getName().str());
      InstDescriptor.set_bb(BBIter->getName().str());
      InstDescriptor.set_pos_in_bb(PosInBB);

      // Insert all the params & result.
      if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        for (unsigned IncomingValueId = 0,
                      NumIncomingValues = PHIInst->getNumIncomingValues();
             IncomingValueId < NumIncomingValues; IncomingValueId++) {
          auto IncomingValue = PHIInst->getIncomingValue(IncomingValueId);
          auto IncomingBlock = PHIInst->getIncomingBlock(IncomingValueId);
          bool IsParam = true;
          auto TypeID = IncomingBlock->getType()->getTypeID();
          auto Value = InstDescriptor.add_values();
          Value->set_is_param(IsParam);
          Value->set_type_id(TypeID);
        }
      } else {
        for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
             OperandId < NumOperands; OperandId++) {
          auto Operand = Inst->getOperand(OperandId);
          bool IsParam = true;
          auto TypeID = Operand->getType()->getTypeID();
          auto Value = InstDescriptor.add_values();
          Value->set_is_param(IsParam);
          Value->set_type_id(TypeID);
        }
      }

      if (Inst->getName() != "") {
        bool IsParam = false;
        auto TypeID = Inst->getType()->getTypeID();
        auto Value = InstDescriptor.add_values();
        Value->set_is_param(IsParam);
        Value->set_type_id(TypeID);
      }

      // Also insert into the InstUIDMap.
      auto Inserted = this->InstUIDMap.emplace(Inst, UID).second;
      assert(Inserted && "Failed to create entry in inst uid map.");
    }
  }
}

void InstructionUIDMap::allocateFunctionUID(const llvm::Function *Func) {
  assert(this->FuncUIDMap.count(Func) == 0 &&
         "Function UID already allocated.");
  auto FuncUIDBase = generateFuncUIDBase(Func);

  auto UIDEmplaceResult = this->FuncUIDUsedMap.emplace(FuncUIDBase, 1);
  if (!UIDEmplaceResult.second) {
    // This FuncUIDBase is already used.
    // Increase the used count.
    UIDEmplaceResult.first->second++;
    // Modify the FuncUID by appending a sequence number.
    FuncUIDBase += std::to_string(UIDEmplaceResult.first->second);
  }

  // Insert the FuncUID.
  LLVM::TDG::FunctionDescriptor FuncDescriptor;
  FuncDescriptor.set_func_name(Func->getName().str());
  auto &FuncUIDMap = *(this->UIDMap.mutable_func_map());
  FuncUIDMap.operator[](FuncUIDBase) = FuncDescriptor;

  this->FuncUIDMap.emplace(Func, FuncUIDBase);
}

std::string InstructionUIDMap::generateFuncUIDBase(const llvm::Function *Func) {
  auto FuncUIDBase = Func->getName().str();
  auto DebugMDNode = Func->getMetadata(llvm::LLVMContext::MD_dbg);
  if (DebugMDNode != nullptr) {
    if (auto DISubprogram = llvm::dyn_cast<llvm::DISubprogram>(DebugMDNode)) {
      if (auto DIFile = DISubprogram->getFile()) {
        std::stringstream SS;
        SS << llvm::sys::path::filename(DIFile->getFilename()).str()
           << "::" << DISubprogram->getLine();
        /**
         * With the source file and line, we should already guarantee
         * the uniqueness of the functions.
         * FIX: OMG I was wrong. Source file + line does not guarantee the
         * uniqueness, due to C++ template function.
         *
         * An adhoc fix: use the first 128 bytes.
         */
        const auto &DemangledFuncName = Utils::getDemangledFunctionName(Func);
        if (DemangledFuncName.size() <= 128) {
          SS << '(' << DemangledFuncName << ')';
        }
        FuncUIDBase = SS.str();
      }
    }
  }
  return FuncUIDBase;
}

void InstructionUIDMap::serializeToTxt(const std::string &FileName) const {
  std::ofstream O(FileName);
  assert(O.is_open() &&
         "Failed to open InstructionUIDMap serialization txt file.");
  const auto &UIDInstDescriptorMap = this->UIDMap.inst_map();
  O << UIDInstDescriptorMap.size() << '\n';
  for (const auto &Record : UIDInstDescriptorMap) {
    O << Record.first << ' ' << Record.second.func() << ' '
      << Record.second.bb() << ' ' << Record.second.pos_in_bb() << ' '
      << Record.second.op() << ' ' << Record.second.values_size();
    for (const auto &Value : Record.second.values()) {
      O << ' ' << Value.is_param() << ' ' << Value.type_id();
    }
    O << '\n';
  }
  O << this->FuncUIDMap.size() << '\n';
  for (const auto &FuncUID : this->FuncUIDMap) {
    O << FuncUID.second << '-' << FuncUID.first->getName().str() << '\n';
  }
  O.close();
}

void InstructionUIDMap::serializeTo(const std::string &FileName) const {
  std::ofstream O(FileName);
  assert(O.is_open() && "Failed to open InstructionUIDMap serialization file.");
  this->UIDMap.SerializeToOstream(&O);
  O.close();
}

void InstructionUIDMap::parseFrom(const std::string &FileName,
                                  llvm::Module *Module) {
  assert(this->UIDMap.inst_map().empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");

  this->UIDMap.ParseFromIstream(&I);
  I.close();

  // Find the real function in the module.
  for (const auto &UIDFuncDescriptor : this->UIDMap.func_map()) {
    const auto &FuncName = UIDFuncDescriptor.second.func_name();
    auto Func = Module->getFunction(FuncName);
    if (Func == nullptr) {
      llvm::errs() << "Failed to look up func " << FuncName << '\n';
    }
    assert(Func && "Failed to look up func from FuncUID.");
    auto Inserted =
        this->FuncUIDMap.emplace(Func, UIDFuncDescriptor.first).second;
    assert(Inserted && "Failed to insert FuncUID.");
  }

  // Find the real llvm instruction in the Module.
  // Memorized map to quickly locate the static instruction.
  std::unordered_map<
      llvm::Function *,
      std::unordered_map<std::string, std::vector<llvm::Instruction *>>>
      MemorizedStaticInstMap;

  for (const auto &UIDDescriptor : this->UIDMap.inst_map()) {

    const auto &FuncName = UIDDescriptor.second.func();
    const auto &BBName = UIDDescriptor.second.bb();
    const auto &PosInBB = UIDDescriptor.second.pos_in_bb();

    llvm::Function *Func = Module->getFunction(FuncName);
    assert(Func && "Failed to look up traced function in module.");

    auto FuncMapIter = MemorizedStaticInstMap.find(Func);
    if (FuncMapIter == MemorizedStaticInstMap.end()) {
      FuncMapIter =
          MemorizedStaticInstMap
              .emplace(std::piecewise_construct, std::forward_as_tuple(Func),
                       std::forward_as_tuple())
              .first;
    }

    auto &BBMap = FuncMapIter->second;
    auto BBMapIter = BBMap.find(BBName);
    if (BBMapIter == BBMap.end()) {
      // Iterate through function's Basic blocks to create all the bbs.
      for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
           ++BBIter) {
        std::string BBName = BBIter->getName().str();
        auto &InstVec =
            BBMap
                .emplace(std::piecewise_construct,
                         std::forward_as_tuple(BBName), std::forward_as_tuple())
                .first->second;
        // After this point BBName is moved.
        for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
             InstIter != InstEnd; ++InstIter) {
          InstVec.push_back(&*InstIter);
        }
      }
      BBMapIter = BBMap.find(BBName);
      if (BBMapIter == BBMap.end()) {
        llvm::errs() << "Failed to find the BB " << BBName << " of Func "
                     << FuncName << '\n';
      }
      assert(BBMapIter != BBMap.end() &&
             "Failed to find the basic block in BBMap.");
    }

    const auto &InstVec = BBMapIter->second;
    assert(PosInBB >= 0 && PosInBB < InstVec.size() && "Invalid Index.");
    auto Inst = InstVec[PosInBB];

    // Insert this into the map.
    auto Inserted = this->UIDInstMap.emplace(UIDDescriptor.first, Inst).second;
    assert(Inserted && "Failed to insert to UIDInstMap.");

    Inserted = this->InstUIDMap.emplace(Inst, UIDDescriptor.first).second;
    assert(Inserted && "Failed to insert to InstUIDMap.");
  }

  // Release the UIDMap as we don't need it anymore.
  this->UIDMap.Clear();
}

llvm::Instruction *InstructionUIDMap::getInst(const InstructionUID UID) const {
  auto Iter = this->UIDInstMap.find(UID);
  assert(Iter != this->UIDInstMap.end() && "Unrecognized UID.");
  return Iter->second;
}

InstructionUIDMap::InstructionUID
InstructionUIDMap::getUID(const llvm::Instruction *Inst) const {
  auto Iter = this->InstUIDMap.find(Inst);
  assert(Iter != this->InstUIDMap.end() && "Unrecognized inst.");
  return Iter->second;
}

bool InstructionUIDMap::hasFuncUID(const llvm::Function *Func) const {
  return this->FuncUIDMap.count(Func);
}

const std::string &
InstructionUIDMap::getFuncUID(const llvm::Function *Func) const {
  auto Iter = this->FuncUIDMap.find(Func);
  assert(Iter != this->FuncUIDMap.end() && "Unrecognized func.");
  return Iter->second;
}
