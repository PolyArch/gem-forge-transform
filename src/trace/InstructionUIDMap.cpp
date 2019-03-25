#include "trace/InstructionUIDMap.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

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

  for (auto BBIter = Func->begin(), BBEnd = Func->end(); BBIter != BBEnd;
       ++BBIter) {
    auto PosInBB = 0;
    for (auto InstIter = BBIter->begin(), InstEnd = BBIter->end();
         InstIter != InstEnd; ++InstIter, ++PosInBB) {
      auto Inst = &*InstIter;
      // Create the value vector.
      std::vector<InstructionValueDescriptor> Values;

      if (auto PHIInst = llvm::dyn_cast<llvm::PHINode>(Inst)) {
        for (unsigned IncomingValueId = 0,
                      NumIncomingValues = PHIInst->getNumIncomingValues();
             IncomingValueId < NumIncomingValues; IncomingValueId++) {
          auto IncomingValue = PHIInst->getIncomingValue(IncomingValueId);
          auto IncomingBlock = PHIInst->getIncomingBlock(IncomingValueId);
          bool IsParam = true;
          auto TypeID = IncomingBlock->getType()->getTypeID();
          Values.emplace_back(IsParam, TypeID);
        }
      } else {
        for (unsigned OperandId = 0, NumOperands = Inst->getNumOperands();
             OperandId < NumOperands; OperandId++) {
          auto Operand = Inst->getOperand(OperandId);
          bool IsParam = true;
          auto TypeID = Operand->getType()->getTypeID();

          Values.emplace_back(IsParam, TypeID);
        }
      }

      if (Inst->getName() != "") {
        bool IsParam = false;
        auto TypeID = Inst->getType()->getTypeID();

        Values.emplace_back(IsParam, TypeID);
      }

      // Return value of str is rvalue, will be moved.
      auto UID = this->AvailableUID;
      this->AvailableUID += InstructionUIDMap::InstSize;
      auto Inserted =
          this->Map
              .emplace(std::piecewise_construct, std::forward_as_tuple(UID),
                       std::forward_as_tuple(
                           std::string(Inst->getOpcodeName()),
                           Inst->getFunction()->getName().str(),
                           BBIter->getName().str(), PosInBB, std::move(Values)))
              .second;
      assert(Inserted && "Failed to create entry in uid descriptor map.");
      // Also insert into the InstUIDMap.
      Inserted = this->InstUIDMap.emplace(Inst, UID).second;
      assert(Inserted && "Failed to create entry in inst uid map.");
    }
  }
}

void InstructionUIDMap::serializeTo(const std::string &FileName) const {
  std::ofstream O(FileName);
  assert(O.is_open() && "Failed to open InstructionUIDMap serialization file.");
  for (const auto &Record : this->Map) {
    O << Record.first << ' ' << Record.second.FuncName << ' '
      << Record.second.BBName << ' ' << Record.second.PosInBB << ' '
      << Record.second.OpName << ' ' << Record.second.Values.size();
    for (const auto &Value : Record.second.Values) {
      O << ' ' << Value.IsParam << ' ' << Value.TypeID;
    }
    O << '\n';
  }
}

void InstructionUIDMap::parseFrom(const std::string &FileName,
                                  llvm::Module *Module) {
  assert(this->Map.empty() &&
         "UIDMap is not empty when try to parse from some file.");
  std::ifstream I(FileName);
  assert(I.is_open() && "Failed to open InstructionUIDMap parse file.");
  InstructionUID UID;
  std::string FuncName, BBName, OpName;
  int PosInBB, NumValues;

  bool IsParam;
  unsigned TypeID;

  while (I >> UID >> FuncName >> BBName >> PosInBB >> OpName >> NumValues) {
    std::vector<InstructionValueDescriptor> Values;
    Values.reserve(NumValues);
    for (int Idx = 0; Idx < NumValues; ++Idx) {
      I >> IsParam >> TypeID;
      Values.emplace_back(IsParam, static_cast<llvm::Type::TypeID>(TypeID));
    }

    // Local variables will be copied.
    auto Inserted =
        this->Map
            .emplace(std::piecewise_construct, std::forward_as_tuple(UID),
                     std::forward_as_tuple(OpName, FuncName, BBName, PosInBB,
                                           Values))
            .second;
    assert(Inserted && "Failed to insert the record.");
  }

  // Find the real llvm instruction in the Module.

  // Memorized map to quickly locate the static instruction.
  std::unordered_map<
      llvm::Function *,
      std::unordered_map<std::string, std::vector<llvm::Instruction *>>>
      MemorizedStaticInstMap;

  for (const auto &UIDDescriptor : this->Map) {

    const auto &FuncName = UIDDescriptor.second.FuncName;
    const auto &BBName = UIDDescriptor.second.BBName;
    const auto &PosInBB = UIDDescriptor.second.PosInBB;

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
}

const InstructionUIDMap::InstructionDescriptor &
InstructionUIDMap::getDescriptor(const InstructionUID UID) const {
  auto Iter = this->Map.find(UID);
  assert(Iter != this->Map.end() && "Unrecognized UID.");
  return Iter->second;
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
