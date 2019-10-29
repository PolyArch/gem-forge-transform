
#include "gem5/m5ops.h"
typedef int Value;

/**
 * Used for random test purpose.
 */

// 65536*8 is 512kB.
const int N = 65536 * 2;
const int STEP_AHEAD = 16;
Value a[N + STEP_AHEAD * 16];

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  volatile Value *a = *pa;
  // Make sure there is no reuse.
  #pragma nounroll
  for (long long i = 0; i < N; i += 16) {
    // Lookahead to trigger IVV (L1_GETS) bug.
    Value lookahead = a[i + STEP_AHEAD * 16];
    sum += a[i];
  }
  return sum;
}


int main() {
  volatile Value c;
  Value *pa = a;
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  m5_detail_sim_start();
  c = foo(&pa, N);
  m5_detail_sim_end();
  return 0;
}
