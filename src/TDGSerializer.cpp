#include "TDGSerializer.h"

TDGSerializer::TDGSerializer(const std::string &FileName)
    : Out(FileName, std::ios::out | std::ios::binary), SerializedInsts(0) {
  assert(this->Out.is_open() && "Failed to open output serialize file.");
}

TDGSerializer::~TDGSerializer() {
  // Serialize the remainning instructions.
  if (this->TDG.instructions_size() > 0) {
    this->SerializedInsts += this->TDG.instructions_size();
    uint64_t Bytes = this->TDG.ByteSizeLong();
    this->Out.write(reinterpret_cast<char *>(&Bytes), sizeof(Bytes));
    this->TDG.SerializeToOstream(&this->Out);
    this->TDG.clear_instructions();
  }
  this->Out.close();
}

void TDGSerializer::serialize(DynamicInstruction *DynamicInst, DataGraph *DG) {
  auto ProtobufEntry = this->TDG.add_instructions();
  DynamicInst->serializeToProtobuf(ProtobufEntry, DG);

  // Serialize every some instructions.
  if (this->TDG.instructions_size() == 10000) {
    this->SerializedInsts += this->TDG.instructions_size();
    uint64_t Bytes = this->TDG.ByteSizeLong();
    this->Out.write(reinterpret_cast<char *>(&Bytes), sizeof(Bytes));
    this->TDG.SerializeToOstream(&this->Out);
    this->TDG.clear_instructions();
  }
}