#ifndef VERTICAL_COMMON_H
#define VERTICAL_COMMON_H

#include "stdint.h"
void m5_reset_stats(uint64_t ns_delay, uint64_t ns_period);
void m5_work_begin(uint64_t workid, uint64_t threadid);
void m5_work_end(uint64_t workid, uint64_t threadid);

#define DETAILED_SIM_START() m5_work_begin(0, 0); m5_reset_stats(0, 0);
#define DETAILED_SIM_STOP() m5_work_end(0, 0);

#endif
