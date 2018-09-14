
#include "trace/TraceParserGZip.h"

#include <iostream>
#include <cassert>

// Implement the parser.
TraceParserGZip::TraceParserGZip(const std::string& TraceFileName) {
  this->TraceFile = gzOpenGZTrace(TraceFileName.c_str());
  assert(this->TraceFile != NULL);
  this->readLine();
}

TraceParserGZip::~TraceParserGZip() { gzCloseGZTrace(this->TraceFile); }

TraceParser::Type TraceParserGZip::getNextType() {
  // Use the first line in the buffer to determine which type it is.
  if (this->TraceFile->LineLen == 0) {
    // This is an empty line.
    // It means that we have reached the end.
    return Type::END;
  }

  switch (this->TraceFile->LineBuf[0]) {
    case 'i': {
      return Type::INST;
    }
    case 'e': {
      return Type::FUNC_ENTER;
    }
    default: { assert(false && "Illegal first line type."); }
  }
}

TraceParser::TracedInst TraceParserGZip::parseLLVMInstruction() {
  assert(this->getNextType() == Type::INST &&
         "The next trace entry is not instruction.");

  TracedInst Parsed;

  // Parse the instruction line.
  std::string Line = this->TraceFile->LineBuf;
  auto InstructionLineFields = splitByChar(Line, '|');
  assert(InstructionLineFields.size() == 5 &&
         "Incorrect fields for instrunction line.");
  assert(InstructionLineFields[0] == "i" && "The first filed must be \"i\".");
  Parsed.Func = InstructionLineFields[1];
  Parsed.BB = InstructionLineFields[2];
  Parsed.Id = std::stoi(InstructionLineFields[3]);
  Parsed.Op = InstructionLineFields[4];

  // Parse the dynamic values.
  while (true) {
    // Read in the next line.
    this->readLine();
    if (isBreakLine(this->TraceFile->LineBuf, this->TraceFile->LineLen)) {
      // We have reached the next entry.
      // Save this for next time.
      break;
    }

    Line = this->TraceFile->LineBuf;
    switch (Line.front()) {
      case 'r': {
        // This is the result line.
        assert(Parsed.Result == "" &&
               "Multiple results for single instruction.");
        auto ResultLineFields = splitByChar(Line, '|');
        Parsed.Result = ResultLineFields[1];
        break;
      }
      case 'p': {
        auto OperandLineFields = splitByChar(Line, '|');
        assert(OperandLineFields.size() >= 2 &&
               "Too few fields for operand line.");
        Parsed.Operands.push_back(OperandLineFields[1]);
        break;
      }
      default: {
        assert(false && "Unknown value type for inst.");
        break;
      }
    }
  }

  return Parsed;
}

TraceParser::TracedFuncEnter TraceParserGZip::parseFunctionEnter() {
  assert(this->getNextType() == Type::FUNC_ENTER &&
         "The next trace entry is not function enter.");

  TracedFuncEnter Parsed;

  // Parse the instruction line.
  std::string Line = this->TraceFile->LineBuf;
  auto EnterLineFields = splitByChar(Line, '|');
  assert(EnterLineFields.size() == 2 &&
         "Incorrect fields for function enter line.");
  assert(EnterLineFields[0] == "e" && "The first filed must be \"e\".");
  Parsed.Func = EnterLineFields[1];

  while (true) {
    this->readLine();
    if (isBreakLine(this->TraceFile->LineBuf, this->TraceFile->LineLen)) {
      // We have reached the next entry.
      break;
    }

    Line = this->TraceFile->LineBuf;

    switch (Line.front()) {
      case 'p': {
        auto ArugmentLineFields = splitByChar(Line, '|');
        assert(ArugmentLineFields.size() >= 2 &&
               "Too few argument line fields.");
        Parsed.Arguments.push_back(ArugmentLineFields[1]);
        break;
      }
      default: {
        assert(false && "Unknown value line for function enter.");
        break;
      }
    }
  }

  return Parsed;
}

void TraceParserGZip::readLine() {
  int LineLen = gzReadNextLine(this->TraceFile);
  assert(LineLen >= 0 && "Something wrong when reading gz trace file.");
}

bool TraceParserGZip::isBreakLine(const char* Line, int LineLen) {
  if (LineLen == 0) {
    return true;
  }
  return Line[0] == 'i' || Line[0] == 'e';
}

/**
 * Split a string like a|b|c| into [a, b, c].
 */
std::vector<std::string> TraceParserGZip::splitByChar(const std::string& source,
                                                      char split) {
  std::vector<std::string> ret;
  size_t idx = 0, prev = 0;
  for (; idx < source.size(); ++idx) {
    if (source[idx] == split) {
      ret.push_back(source.substr(prev, idx - prev));
      prev = idx + 1;
    }
  }
  // Don't miss the possible last field.
  if (prev < idx) {
    ret.push_back(source.substr(prev, idx - prev));
  }
  return ret;
}