#ifndef LLVM_TDG_LOGGER_H
#define LLVM_TDG_LOGGER_H

#include <stdint.h>
#include <zlib.h>

// Helper function to manually handle the buffer of gzfile.
#define GZ_TRACE_BUF_LEN 256
#define GZ_TRACE_LINE_MAX_LEN 512
typedef struct GZTraceT {
  char PingPongBuffer[2 * GZ_TRACE_BUF_LEN];
  char LineBuf[GZ_TRACE_LINE_MAX_LEN];
  int LineLen;
  gzFile File;
  uint64_t HeadIdx;
  uint64_t TailIdx;
} GZTrace;

// Return NULL if failed.
GZTrace* gzOpenGZTrace(const char* FileName);
void gzCloseGZTrace(GZTrace* Trace);
int gzReadNextLine(GZTrace* Trace);

#endif