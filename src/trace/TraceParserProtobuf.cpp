#include "trace/TraceParserProtobuf.h"

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>

#define DEBUG_TYPE "ParserProtobuf"

TraceParserProtobuf::TraceParserProtobuf(const std::string &TraceFileName,
                                         const std::string &InstUIDMapFileName)
    : TraceFile(TraceFileName, std::ios::in | std::ios::binary), Count(0) {
  if (InstUIDMapFileName != "") {
    this->InstUIDMap.parseFrom(InstUIDMapFileName);
  }

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
  Parsed.Result = this->TraceEntry.inst().result();

  for (int i = 0; i < this->TraceEntry.inst().params_size(); ++i) {
    Parsed.Operands.emplace_back(this->TraceEntry.inst().params(i));
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
    Parsed.Arguments.emplace_back(this->TraceEntry.func_enter().params(i));
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

  // int ReadSize = 0;
  // char Data[sizeof(uint64_t)];
  // const char *Buffer;
  // int Size;
  // while (this->IStream->Next((const void **)&Buffer, &Size)) {
  //   // We got something.
  //   int Idx = 0;
  //   while (ReadSize < sizeof(uint64_t) && Idx < Size) {
  //     Data[ReadSize] = Buffer[Idx];
  //     ReadSize++;
  //     Idx++;
  //   }
  //   if (Size - Idx > 0) {
  //     // Rewind unused data.
  //     this->IStream->BackUp(Size - Idx);
  //   }
  //   if (ReadSize == sizeof(uint64_t)) {
  //     // We have read in the size field.
  //     bool Success = this->TraceEntry.ParseFromBoundedZeroCopyStream(
  //         this->IStream, *(uint64_t *)(Data));
  //     if (!Success) {
  //       std::cerr << "Failed parsing " << *(uint64_t *)Data << std::endl;
  //     }
  //     assert(Success && "Failed parsing protobuf.");
  //     Count++;
  //     break;
  //   }
  // }
}

#undef DEBUG_TYPE