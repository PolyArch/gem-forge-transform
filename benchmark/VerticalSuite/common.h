#ifndef VERTICAL_COMMON_H
#define VERTICAL_COMMON_H

#include "stdint.h"
void m5_reset_stats(uint64_t ns_delay, uint64_t ns_period);
void m5_work_begin(uint64_t workid, uint64_t threadid);
void m5_work_end(uint64_t workid, uint64_t threadid);

#define m5_detail_sim_start() m5_work_begin(0, 0); m5_reset_stats(0, 0);
#define m5_detail_sim_end() m5_work_end(0, 0)

#endif
