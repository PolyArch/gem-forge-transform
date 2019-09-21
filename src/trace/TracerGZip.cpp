#include "TracerImpl.h"

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

void switchFileImpl(int tid) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  cleanup();
}

#define log(...)                                                               \
  {                                                                            \
    if (p == NULL) {                                                           \
      p = gzopen(getNewTraceFileName(0), OpenMode);                             \
      atexit(&cleanup);                                                        \
    }                                                                          \
    assert(p != NULL);                                                         \
    gzprintf(p, __VA_ARGS__);                                                  \
  }

void printFuncEnterImpl(int tid, const char *FunctionName) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("e|%s|\n", FunctionName);
}

void printInstImpl(int tid, const char *FunctionName, const char *BBName,
                   unsigned Id, uint64_t UID, const char *OpCodeName) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("i|%s|%s|%d|%s|\n", FunctionName, BBName, Id, OpCodeName);
}

void printValueLabelImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|%s|\n", Tag, Name);
}
void printValueIntImpl(int tid, const char Tag, const char *Name,
                       unsigned TypeId, uint64_t Value) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|%" PRIu64 "|\n", Tag, Value);
}
void printValueFloatImpl(int tid, const char Tag, const char *Name,
                         unsigned TypeId, double Value) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|%f|\n", Tag, Value);
}
void printValuePointerImpl(int tid, const char Tag, const char *Name,
                           unsigned TypeId, void *Value) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|0x%llx|\n", Tag, (unsigned long long)Value);
}
void printValueVectorImpl(int tid, const char Tag, const char *Name,
                          unsigned TypeId, uint32_t Size, uint8_t *Value) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|", Tag);
  for (uint32_t i = 0; i < Size; ++i) {
    log("%hhu,", Value[i]);
  }
  log("|\n");
}
void printValueUnsupportImpl(int tid, const char Tag, const char *Name,
                             unsigned TypeId) {
  assert(tid == 0 && "TracerGZip only support single thread.");
  log("%c|Unsupported(%u)|\n", TypeId);
}
// Do nothing for the wrap function.
void printInstEndImpl(int tid) {
  assert(tid == 0 && "TracerGZip only support single thread.");
}
void printFuncEnterEndImpl(int tid) {
  assert(tid == 0 && "TracerGZip only support single thread.");
}
