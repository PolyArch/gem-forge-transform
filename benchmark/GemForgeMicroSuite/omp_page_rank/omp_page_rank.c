/**
 * Simple array sum.
 */
#ifdef GEM_FORGE
#include "gem5/m5ops.h"
#endif

#include <assert.h>
#include <malloc.h>
#include <math.h>
#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef uint32_t IndexT;
typedef struct {
  float rank;
  float next;
} Value;

IndexT numNodes, numEdges;
IndexT *edgePos;
IndexT *edgeList;
Value *values;

void readInGraph(const char *fn) {
  FILE *f = fopen(fn, "rb");
  fread(&numNodes, sizeof(numNodes), 1, f);
  fread(&numEdges, sizeof(numEdges), 1, f);

  // We make the last pos numEdges so that we can calculate out-degree.
  edgePos =
      aligned_alloc(sizeof(edgePos[0]), sizeof(edgePos[0]) * (numNodes + 1));
  fread(edgePos, sizeof(edgePos[0]), numNodes, f);
  edgePos[numNodes] = numEdges;

  edgeList = aligned_alloc(sizeof(edgeList[0]), sizeof(edgeList[0]) * numEdges);
  fread(edgeList, sizeof(edgeList[0]), numEdges, f);
  values = aligned_alloc(sizeof(Value), sizeof(Value) * numNodes);
  for (uint64_t i = 0; i < numNodes; ++i) {
    values[i].rank = 1.0f / numNodes;
    values[i].next = 0.15f / numNodes;
  }
#ifndef GEM_FORGE
  for (uint64_t i = 0; i < numEdges; ++i) {
    if (edgeList[i] >= numNodes) {
      printf("Illegal %u.\n", edgeList[i]);
    }
  }
#endif
  printf("NumNodes %u NumEdges %u Pos %luMB Node %luMB Value %luMB.\n",
         numNodes, numEdges, sizeof(edgePos[0]) * numNodes / 1024 / 1024,
         sizeof(edgeList[0]) * numEdges / 1024 / 1024,
         sizeof(Value) * numNodes / 1024 / 1024);
}

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) int foo(uint64_t numNodes, IndexT *nodes,
                                  IndexT *edges, Value *values, int iters) {

  float global_diff = 2.0f;
  int iter = 0;

  for (iter = 0; iter < iters && global_diff > 1.0f; ++iter) {
#ifdef GEM_FORGE
    m5_work_begin(0, 0);
#endif

#pragma omp parallel for schedule(static)                                      \
    firstprivate(numNodes, nodes, edges, values)
    for (uint64_t i = 0; i < numNodes; ++i) {
      uint64_t start = nodes[i];
      uint64_t end = nodes[i + 1];
      float rank = values[i].rank;
      float out_degree = (float)(end - start);
      float delta = 0.85 * rank / out_degree;
#pragma clang loop unroll(disable) interleave(disable)
      for (uint64_t j = start; j < end; ++j) {
        uint64_t n = edges[j];
        __atomic_fetch_fadd(&values[n].next, delta, __ATOMIC_RELAXED);
      }
    }

#ifdef GEM_FORGE
    m5_work_end(0, 0);
    m5_work_begin(1, 0);
#endif

    global_diff = 0.0f;

#pragma omp parallel for schedule(static)                                      \
    firstprivate(numNodes, nodes, edges, values)
    for (uint64_t i = 0; i < numNodes; ++i) {
      float rank = values[i].rank;
      float next = values[i].next;
      float diff = fabs(next - rank);
      values[i].rank = next;
      values[i].next = 0.15f / numNodes;
      __atomic_fetch_fadd(&global_diff, diff, __ATOMIC_RELAXED);
    }

#ifdef GEM_FORGE
    m5_work_end(1, 0);
#endif
  }

  return iter;
}

#define CACHE_BLOCK_SIZE 64

int main(int argc, char *argv[]) {

  assert(argc == 3);
  int numThreads = atoi(argv[1]);
  printf("Number of Threads: %d.\n", numThreads);

  char *graphFn = argv[2];
  readInGraph(graphFn);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif

#ifdef WARM_CACHE
// This should warm up the cache.
#define WARM_UP(a, n)                                                          \
  for (int i = 0; i < sizeof(*(a)) * n; i += 64) {                             \
    volatile char x = ((char *)(a))[i];                                        \
  }
  WARM_UP(edgePos, numNodes);
  WARM_UP(edgeList, numEdges);
  WARM_UP(values, numNodes);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = values[tid];
  }
#endif
#ifdef GEM_FORGE
  m5_reset_stats(0, 0);
#endif

  volatile int ret = foo(numNodes, edgePos, edgeList, values, 10);
#ifdef GEM_FORGE
  m5_detail_sim_end();
  exit(0);
#endif

#ifdef CHECK
  // Value expected = 0;
  // for (int i = 0; i < N; i += STRIDE) {
  //   expected += a[i];
  // }
  // expected *= NUM_THREADS;
  printf("Ret = %d.\n", ret);
#endif

  return 0;
}
