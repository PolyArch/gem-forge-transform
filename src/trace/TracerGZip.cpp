#include "trace/Tracer.h"

#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <zlib.h>

static gzFile p = NULL;
static const char *OpenMode = "w";

void cleanup() {
  if (p != NULL) {
    gzclose(p);
    p = NULL;
  }
}

void switchFileImpl() { cleanup(); }

#define log(...)                                                               \
  {                                                                            \
    if (p == NULL) {                                                           \
      p = gzopen(getNewTraceFileName(), OpenMode);                             \
      atexit(&cleanup);                                                        \
    }                                                                          \
    assert(p != NULL);                                                         \
    gzprintf(p, __VA_ARGS__);                                                  \
  }

void printFuncEnterImpl(const char *FunctionName) {
  log("e|%s|\n", FunctionName);
}

void printInstImpl(const char *FunctionName, const char *BBName, unsigned Id,
                   uint64_t UID, char *OpCodeName) {
  log("i|%s|%s|%d|%s|\n", FunctionName, BBName, Id, OpCodeName);
}

void printValueLabelImpl(const char Tag, const char *Name, unsigned TypeId) {
  log("%c|%s|\n", Tag, Name);
}
void printValueIntImpl(const char Tag, const char *Name, unsigned TypeId,
                       uint64_t Value) {
  log("%c|%" PRIu64 "|\n", Tag, Value);
}
void printValueFloatImpl(const char Tag, const char *Name, unsigned TypeId,
                         double Value) {
  log("%c|%f|\n", Tag, Value);
}
void printValuePointerImpl(const char Tag, const char *Name, unsigned TypeId,
                           void *Value) {
  log("%c|0x%llx|\n", Tag, (unsigned long long)Value);
}
void printValueVectorImpl(const char Tag, const char *Name, unsigned TypeId,
                          uint32_t Size, uint8_t *Value) {
  log("%c|", Tag);
  for (uint32_t i = 0; i < Size; ++i) {
    log("%hhu,", Value[i]);
  }
  log("|\n");
}
void printValueUnsupportImpl(const char Tag, const char *Name,
                             unsigned TypeId) {
  log("%c|Unsupported(%u)|\n", TypeId);
}
// Do nothing for the wrap function.
void printInstEndImpl() {}
void printFuncEnterEndImpl() {}
