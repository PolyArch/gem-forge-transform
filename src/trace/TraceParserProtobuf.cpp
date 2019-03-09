#include "trace/TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

#define DEBUG_TYPE "ParserProtobuf"

TraceParserProtobuf::TraceParserProtobuf(const std::string &TraceFileName,
                                         const InstructionUIDMap &_InstUIDMap)
    : TraceFile(TraceFileName, std::ios::in | std::ios::binary),
      InstUIDMap(_InstUIDMap),
      Count(0) {
  assert(this->TraceFile.is_open() && "Failed openning trace file.");
  this->IStream =
      new google::protobuf::io::IstreamInputStream(&this->TraceFile);
  this->GzipIStream = new google::protobuf::io::GzipInputStream(this->IStream);
  this->CodedIStream =
      new google::protobuf::io::CodedInputStream(this->GzipIStream);
  this->TraceEntry.Clear();
  this->readNextEntry();
}

TraceParserProtobuf::~TraceParserProtobuf() {
  std::cerr << "Parsed " << Count << std::endl;
  delete this->CodedIStream;
  delete this->GzipIStream;
  delete this->IStream;
  this->TraceFile.close();
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
    default: { return std::string(""); }
  }
}

TraceParser::TracedInst TraceParserProtobuf::parseLLVMInstruction() {
  assert(this->getNextType() == Type::INST &&
         "The next trace entry is not instruction.");
  TracedInst Parsed;

  if (this->TraceEntry.inst().uid() != 0) {
    // Translate the UID back.
    auto InstDescriptor =
        this->InstUIDMap.getDescriptor(this->TraceEntry.inst().uid());
    Parsed.Func = InstDescriptor.FuncName;
    Parsed.BB = InstDescriptor.BBName;
    Parsed.Id = InstDescriptor.PosInBB;
    Parsed.Op = InstDescriptor.OpName;
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

  // Read a message from the stream by getting the size, using it as
  // a limit when parsing the message, then popping the limit again
  uint32_t Size;

  if (this->CodedIStream->ReadVarint32(&Size)) {
    auto Limit = this->CodedIStream->PushLimit(Size);
    if (this->TraceEntry.ParseFromCodedStream(this->CodedIStream)) {
      this->CodedIStream->PopLimit(Limit);
      // All went well, the message is parsed and the limit is
      // popped again
      this->Count++;
      return;
    } else {
      assert(false && "Failed parsing protobuf.");
    }
  }
}

#undef DEBUG_TYPE
