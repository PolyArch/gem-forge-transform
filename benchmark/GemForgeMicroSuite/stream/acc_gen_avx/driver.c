
#include "gfm_utils.h"

int main(int argc, char *argv[]) {

  int numThreads = 1;
  int check = 0;
  int warm = 0;
  int argx = 2;

  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    warm = atoi(argv[argx - 1]);
  }
  argx++;
  uint64_t T = M * N + L * N + L * M;
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB.\n", T * sizeof(Value) / 1024);

#ifndef NO_OPENMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  uint64_t totalBytes = T * sizeof(Value) + OFFSET_BYTES;
  uint64_t numPages = (totalBytes + PAGE_SIZE - 1) / PAGE_SIZE;
  Value *buffer = (Value *)aligned_alloc(PAGE_SIZE, numPages * PAGE_SIZE);

  // Initialize separately so that their physical address is not interleaved
  int elementsPerPage = PAGE_SIZE / sizeof(Value);
#pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
  for (int i = 0; i < numPages; i++) {
    volatile Value v = buffer[i * elementsPerPage];
  }

  // A X = B
  Value *a = buffer;
  Value *b = a + L * M;
  Value *c = b + M * N + (OFFSET_BYTES / sizeof(Value));

#ifdef GEM_FORGE
  gf_stream_nuca_region("gfm.mm_outer.a", a, sizeof(a[0]), M, L);
  gf_stream_nuca_region("gfm.mm_outer.b", b, sizeof(b[0]), N, M);
  gf_stream_nuca_region("gfm.mm_outer.c", c, sizeof(c[0]), N, L);
  gf_stream_nuca_align(a, a, 1);
  gf_stream_nuca_align(a, a, M);
  gf_stream_nuca_align(b, a, 0);
  gf_stream_nuca_align(c, a, 0);
  gf_stream_nuca_set_property(a, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_set_property(b, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_set_property(c, STREAM_NUCA_REGION_PROPERTY_BROADCAST_DIM, 0);
  gf_stream_nuca_remap();
#endif

  assert(!check && "Cannot check.");

  gf_detail_sim_start();
  if (warm) {
    gf_warm_array("gfm.mm_outer.a", a, L * M * sizeof(a[0]));
    gf_warm_array("gfm.mm_outer.b", b, M * N * sizeof(b[0]));
    gf_warm_array("gfm.mm_outer.c", c, L * N * sizeof(c[0]));
  }

#ifndef NO_OPENMP
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = a[tid];
  }
#endif

  gf_reset_stats();
  volatile Value computed = driver(a, b, c, L, M, N);
  gf_detail_sim_end();

  return 0;
}
