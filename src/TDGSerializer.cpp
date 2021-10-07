#include "TDGSerializer.h"

#include <google/protobuf/text_format.h>

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "TDGSerializer"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

TDGSerializer::TDGSerializer(const std::string &_FileName, bool TextMode)
    : FileName(_FileName), Gem5Serializer(_FileName), TextSerializer(nullptr),
      SerializedStaticInfomation(false), SerializedInsts(0) {
  if (TextMode) {
    this->TextSerializer = new TextProtobufSerializer(_FileName + ".txt");
  }
}

TDGSerializer::~TDGSerializer() {
  // Serialize the remainning instructions.
  if (this->TDG.instructions_size() > 0) {
    this->write();
  }

  if (this->TextSerializer != nullptr) {
    delete this->TextSerializer;
    this->TextSerializer = nullptr;
  }

  llvm::errs() << "Serialized " << this->SerializedInsts << " to "
               << this->FileName << '\n';
}

void TDGSerializer::serializeStaticInfo(
    const LLVM::TDG::StaticInformation &StaticInfo) {
  assert(!this->SerializedStaticInfomation &&
         "StaticInfomation already serialized.");
  this->Gem5Serializer.serialize(StaticInfo);
  if (this->TextSerializer != nullptr) {
    this->TextSerializer->serialize(StaticInfo);
  }
  this->SerializedStaticInfomation = true;
}

void TDGSerializer::serialize(DynamicInstruction *DynamicInst, DataGraph *DG) {
  auto ProtobufEntry = this->TDG.add_instructions();
  DynamicInst->serializeToProtobuf(ProtobufEntry, DG);

  // Serialize every some instructions.
  if (this->TDG.instructions_size() == 10000) {
    this->write();
  }
}

void TDGSerializer::write() {
  assert(this->SerializedStaticInfomation &&
         "Should serialize the static information first.");
  assert(this->TDG.instructions_size() > 0 &&
         "Nothing to write for TDG serializer.");
  // Hacky to dump some serialized instructions for debug.
  const uint64_t DUMP_START = 0;
  const uint64_t DUMP_END = 1100;
  if (this->SerializedInsts < DUMP_END ||
      this->SerializedInsts + this->TDG.instructions_size() >= DUMP_START) {
    for (uint64_t DUMPED = 0; DUMPED < this->TDG.instructions_size();
         ++DUMPED) {
      auto Id = DUMPED + this->SerializedInsts;
      if (Id < DUMP_START) {
        continue;
      }
      if (Id >= DUMP_END) {
        break;
      }
      std::string OutString;
      LLVM_DEBUG(google::protobuf::TextFormat::PrintToString(
          this->TDG.instructions(DUMPED), &OutString));
      LLVM_DEBUG(llvm::dbgs() << OutString << '\n');
    }
  }

  // Instead of writing them together, let's write one by one.
  for (size_t I = 0; I < this->TDG.instructions_size(); ++I) {
    auto &TDGInstruction = this->TDG.instructions(I);
    this->Gem5Serializer.serialize(TDGInstruction);
    if (this->TextSerializer != nullptr) {
      this->TextSerializer->serialize(TDGInstruction);
    }
  }

  this->SerializedInsts += this->TDG.instructions_size();
  this->TDG.clear_instructions();
}

#undef DEBUG_TYPE