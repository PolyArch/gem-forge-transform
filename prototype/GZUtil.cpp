#include "GZUtil.h"

#include <stdlib.h>

GZTrace* gzOpenGZTrace(const char* FileName) {
  gzFile File = gzopen(FileName, "r");
  if (File == NULL) {
    return NULL;
  }

  GZTrace* Trace = (GZTrace*)malloc(sizeof(GZTrace));
  if (Trace == NULL) {
    return NULL;
  }

  Trace->File = File;
  Trace->HeadIdx = 0;
  Trace->TailIdx = 0;

  return Trace;
}

void gzCloseGZTrace(GZTrace* Trace) {
  gzclose(Trace->File);
  free(Trace);
}

// Helper function to manage the ping pong buffer.
// Ping and Pong form an infinite stream one after another.
// --------------------------------> VIdx
// Ping -- Pong -- Ping -- Pong -- ...
static uint64_t gzComputeRealOffset(uint64_t VIdx) {
  return VIdx % (2 * GZ_TRACE_BUF_LEN);
}

static char gzGetChar(GZTrace* Trace, uint64_t VIdx) {
  return Trace->PingPongBuffer[gzComputeRealOffset(VIdx)];
}

int gzReadNextLine(GZTrace* Trace) {
  // Check if we still have a line in the current buffer.
  int LineLen = 0;
  while (1) {
    if (Trace->HeadIdx == Trace->TailIdx) {
      // We have reached the end.
      // Try fetch something from file.
      uint64_t TailOffset = gzComputeRealOffset(Trace->TailIdx);
      int ReadInLen = gzread(Trace->File, Trace->PingPongBuffer + TailOffset,
                             GZ_TRACE_BUF_LEN);
      if (ReadInLen <= 0) {
        // Something happened.
        if (gzeof(Trace->File)) {
          // We have reached the end.
          // Report the current LineLen.
          break;
        } else {
          return -1;
        }
      }
      // We have successfully readin some content.
      Trace->TailIdx += ReadInLen;
    }

    if (LineLen + 1 >= GZ_TRACE_LINE_MAX_LEN) {
      return -1;
    }

    Trace->LineBuf[LineLen] = gzGetChar(Trace, Trace->HeadIdx);
    Trace->HeadIdx++;

    if (Trace->LineBuf[LineLen] == '\n') {
      // We found a new line.
      break;
    } else {
      LineLen++;
    }
  }

  // Add the 0 terminator. Replace the '\n'
  Trace->LineBuf[LineLen] = 0;
  return LineLen;
}