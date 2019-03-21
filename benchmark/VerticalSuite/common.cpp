#include "stdint.h"
extern "C" {
void m5_reset_stats(uint64_t ns_delay, uint64_t ns_period) {}
void m5_work_begin(uint64_t workid, uint64_t threadid) {}
void m5_work_end(uint64_t workid, uint64_t threadid) {}
}
