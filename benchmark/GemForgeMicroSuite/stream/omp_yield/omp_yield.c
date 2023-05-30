/**
 * Simple array sum.
 */
#include "gem5/m5ops.h"
#include <omp.h>
#include <stdio.h>

#include "../../gfm_utils.h"

typedef int Value;

#define STRIDE 16
#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(int N) {
#pragma omp parallel for
  for (int i = 0; i < N; i += STRIDE) {
    if (i == 0) {
#pragma clang loop unroll(disable)
      for (int j = 0; j < 1000; ++j) {
        printf("iter = %d %d.\n", i, j);
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t offsetBytes = 0;
  uint64_t N = 2;
  int argx = 2;
  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    N = atoll(argv[argx - 1]);
  }
  argx++;

  gf_detail_sim_start();

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  startThreads(numThreads);

  m5_reset_stats(0, 0);
  volatile Value c = foo(N);
  m5_detail_sim_end();
  exit(0);

  return 0;
}
