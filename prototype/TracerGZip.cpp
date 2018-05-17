
#include "Tracer.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <zlib.h>

static gzFile p = NULL;
static const char* OpenMode = "w";

void cleanup() {
  if (p != NULL) {
    gzclose(p);
  }
}

#define log(...)                           \
  {                                        \
    if (p == NULL) {                       \
      p = gzopen(TRACE_FILE_NAME, OpenMode); \
      atexit(&cleanup);                    \
    }                                      \
    assert(p != NULL);                     \
    gzprintf(p, __VA_ARGS__);              \
  }

void printFuncEnter(const char* FunctionName) { log("e|%s|\n", FunctionName); }

void printInst(const char* FunctionName, const char* BBName, unsigned Id,
               char* OpCodeName) {
  log("i|%s|%s|%d|%s|\n", FunctionName, BBName, Id, OpCodeName);
}
/**
 * Print a value.
 * @param Tag: Can be either param or result.
 */
void printValue(const char Tag, const char* Name, unsigned TypeId,
                unsigned NumAdditionalArgs, ...) {
  log("%c|", Tag);
  va_list VAList;
  va_start(VAList, NumAdditionalArgs);
  switch (TypeId) {
    case TypeID::LabelTyID: {
      // For label, log the name again to be compatible with other type.
      log("%s|", Name);
      break;
    }
    case TypeID::IntegerTyID: {
      unsigned value = va_arg(VAList, unsigned);
      log("%u|", value);
      break;
    }
    // Float is promoted to double on x64.
    case TypeID::FloatTyID:
    case TypeID::DoubleTyID: {
      double value = va_arg(VAList, double);
      log("%f|", value);
      break;
    }
    case TypeID::PointerTyID: {
      void* value = va_arg(VAList, void*);
      log("%p|", value);
      break;
    }
    case TypeID::VectorTyID: {
      uint32_t size = va_arg(VAList, uint32_t);
      uint8_t* buffer = va_arg(VAList, uint8_t*);
      for (uint32_t i = 0; i < size; ++i) {
        log("%hhu,", buffer[i]);
      }
      log("|");
      break;
    }
    default: {
      log("UnsupportedType|");
      break;
    }
  }
  va_end(VAList);
  log("\n");
}

// Do nothing for the wrap function.
void printInstEnd() {}