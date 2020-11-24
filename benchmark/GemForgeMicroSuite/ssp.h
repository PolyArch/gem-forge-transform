#ifndef STREAM_ISA_H
#define STREAM_ISA_H

#include <stdint.h>

/****************************************************************
 * These are used as temporary StreamISA API for programmer.
 ****************************************************************/

typedef int32_t StreamIdT;
StreamIdT ssp_config(int step, void *start);
void ssp_end(StreamIdT stream);
int32_t ssp_load_i32(StreamIdT stream);
void ssp_step(StreamIdT stream, int step);

#endif