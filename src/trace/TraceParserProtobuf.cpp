#include "trace/TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

#define DEBUG_TYPE "ParserProtobuf"

TraceParserProtobuf::TraceParserProtobuf(const std::string &TraceFileName)
    : TraceFile(TraceFileName, std::ios::in | std::ios::binary), Count(0) {
  assert(this->TraceFile.is_open() && "Failed openning trace file.");
  this->IStream =
      new google::protobuf::io::IstreamInputStream(&this->TraceFile);
  this->TraceEntry.Clear();
  this->readNextEntry();
}

TraceParserProtobuf::~TraceParserProtobuf() {
  std::cerr << "Parsed " << Count << std::endl;
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

TraceParser::TracedInst TraceParserProtobuf::parseLLVMInstruction() {
  assert(this->getNextType() == Type::INST &&
         "The next trace entry is not instruction.");
  TracedInst Parsed;

  Parsed.Func = this->TraceEntry.inst().func();
  Parsed.BB = this->TraceEntry.inst().bb();
  Parsed.Id = this->TraceEntry.inst().id();
  Parsed.Result = this->TraceEntry.inst().result();
  Parsed.Op = this->TraceEntry.inst().op();

  for (int i = 0; i < this->TraceEntry.inst().params_size(); ++i) {
    Parsed.Operands.emplace_back(this->TraceEntry.inst().params(i));
  }

  // Read the next one.
  this->readNextEntry();
  return std::move(Parsed);
}

TraceParser::TracedFuncEnter TraceParserProtobuf::parseFunctionEnter() {
  assert(this->getNextType() == Type::FUNC_ENTER &&
         "The next trace entry is not function enter.");
  TracedFuncEnter Parsed;

  Parsed.Func = this->TraceEntry.func_enter().func();

  for (int i = 0; i < this->TraceEntry.func_enter().params_size(); ++i) {
    Parsed.Arguments.emplace_back(this->TraceEntry.func_enter().params(i));
  }

  // Read the next one.
  this->readNextEntry();
  return std::move(Parsed);
}

void TraceParserProtobuf::readNextEntry() {
  // TODO: Consider using LimitZeroCopyStream from protobuf.
  this->TraceEntry.Clear();
  int ReadSize = 0;
  char Data[sizeof(uint64_t)];
  const char *Buffer;
  int Size;
  while (this->IStream->Next((const void **)&Buffer, &Size)) {
    // We got something.
    int Idx = 0;
    while (ReadSize < sizeof(uint64_t) && Idx < Size) {
      Data[ReadSize] = Buffer[Idx];
      ReadSize++;
      Idx++;
    }
    if (Size - Idx > 0) {
      // Rewind unused data.
      this->IStream->BackUp(Size - Idx);
    }
    if (ReadSize == sizeof(uint64_t)) {
      // We have read in the size field.
      bool Success = this->TraceEntry.ParseFromBoundedZeroCopyStream(
          this->IStream, *(uint64_t *)(Data));
      if (!Success) {
        std::cerr << "Failed parsing " << *(uint64_t *)Data << std::endl;
      }
      assert(Success && "Failed parsing protobuf.");
      Count++;
      break;
    }
  }
}

#undef DEBUG_TYPE