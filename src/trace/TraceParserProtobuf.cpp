#include "TraceParserProtobuf.h"
#include "../ProtobufSerializer.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

#define DEBUG_TYPE "ParserProtobuf"
#if !defined(LLVM_DEBUG) && defined(DEBUG)
#define LLVM_DEBUG DEBUG
#endif

TraceParserProtobuf::TraceParserProtobuf(const std::string &TraceFileName,
                                         const InstructionUIDMap &_InstUIDMap)
    : Reader(new GzipMultipleProtobufReader(TraceFileName)),
      InstUIDMap(_InstUIDMap), Count(0) {
  this->TraceEntry.Clear();
  this->readNextEntry();
}

TraceParserProtobuf::~TraceParserProtobuf() {
  std::cerr << "Parsed " << Count << std::endl;
  delete this->Reader;
  this->Reader = nullptr;
}

TraceParser::Type TraceParserProtobuf::getNextType() {
  if (!this->TraceEntry.IsInitialized()) {
    return Type::END;
  }
  if (this->TraceEntry.has_inst()) {
    return Type::INST;
  }
  if (this->TraceEntry.has_func_enter()) {
    return Type::FUNC_ENTER;
  }
  std::cerr << "Parsed " << Count << std::endl;
  return Type::END;
}

std::string TraceParserProtobuf::parseLLVMDynamicValueToString(
    const ::LLVM::TDG::DynamicLLVMValue &Value) {
  switch (Value.value_case()) {
  case ::LLVM::TDG::DynamicLLVMValue::ValueCase::kVInt: {
    uint64_t v_int = Value.v_int();
    return std::string(reinterpret_cast<char *>(&v_int), sizeof(v_int));
    break;
  }
  case ::LLVM::TDG::DynamicLLVMValue::ValueCase::kVBytes: {
    return Value.v_bytes();
    break;
  }
  default: {
    return std::string("");
  }
  }
}

TraceParser::TracedInst TraceParserProtobuf::parseLLVMInstruction() {
  assert(this->getNextType() == Type::INST &&
         "The next trace entry is not instruction.");
  TracedInst Parsed;

  if (this->TraceEntry.inst().uid() != 0) {
    // Translate the UID back.
    Parsed.StaticInst = this->InstUIDMap.getInst(this->TraceEntry.inst().uid());
  } else {
    assert(false && "Failed to find instruction uid.");
  }
  Parsed.Result =
      this->parseLLVMDynamicValueToString(this->TraceEntry.inst().result());

  for (int i = 0; i < this->TraceEntry.inst().params_size(); ++i) {
    Parsed.Operands.emplace_back(
        this->parseLLVMDynamicValueToString(this->TraceEntry.inst().params(i)));
  }

  // Read the next one.
  this->readNextEntry();
  return Parsed;
}

TraceParser::TracedFuncEnter TraceParserProtobuf::parseFunctionEnter() {
  assert(this->getNextType() == Type::FUNC_ENTER &&
         "The next trace entry is not function enter.");
  TracedFuncEnter Parsed;

  Parsed.Func = this->TraceEntry.func_enter().func();

  for (int i = 0; i < this->TraceEntry.func_enter().params_size(); ++i) {
    Parsed.Arguments.emplace_back(this->parseLLVMDynamicValueToString(
        this->TraceEntry.func_enter().params(i)));
  }

  // Read the next one.
  this->readNextEntry();
  return Parsed;
}

void TraceParserProtobuf::readNextEntry() {
  // TODO: Consider using LimitZeroCopyStream from protobuf.
  this->TraceEntry.Clear();

  this->Reader->read(this->TraceEntry);
}

#undef DEBUG_TYPE
