/**
 * Simple synchronious page rank. 
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

#include "../gfm_graph_utils.h"
#include "../../gfm_utils.h"

GraphCSR graph;

// Value for computation.
typedef struct {
  float rank;
  float next;
} Value;
Value *values;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) int foo(GraphCSR *graph, Value *values, int iters) {

  float global_diff = 2.0f;
  int iter = 0;

  const GraphIndexT numNodes = graph->numNodes;
  const GraphIndexT *nodes = graph->edgePtr;
  const GraphIndexT *edges = graph->edges;

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
#pragma omp parallel
    {
      float local_diff = 0.0f;
      // Disable kernel2 for now.

      // #pragma omp for schedule(static) firstprivate(numNodes, nodes, edges,
      // values)
      //       for (uint64_t i = 0; i < numNodes; ++i) {
      //         float rank = values[i].rank;
      //         float next = values[i].next;
      //         float diff = fabs(next - rank);
      //         values[i].rank = next;
      //         values[i].next = 0.15f / numNodes;
      //         local_diff += diff;
      //       }

      __atomic_fetch_fadd(&global_diff, local_diff, __ATOMIC_RELAXED);
    }

#ifdef GEM_FORGE
    m5_work_end(1, 0);
#endif
  }

  return iter;
}

// Initialize graph.
void initializeGraph(const char *fn) {
  graph = readInGraph(fn);
  values = aligned_alloc(sizeof(Value), sizeof(Value) * graph.numNodes);
  for (uint64_t i = 0; i < graph.numNodes; ++i) {
    values[i].rank = 1.0f / graph.numNodes;
    values[i].next = 0.15f / graph.numNodes;
  }
  printf("Value %luMB.\n", sizeof(Value) * graph.numNodes / 1024 / 1024);
}

int main(int argc, char *argv[]) {

  assert(argc == 3);
  int numThreads = atoi(argv[1]);
  printf("Number of Threads: %d.\n", numThreads);

  char *graphFn = argv[2];
  initializeGraph(graphFn);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

#ifdef GEM_FORGE
  m5_detail_sim_start();
#endif

#ifdef WARM_CACHE
  // This should warm up the cache.
  WARM_UP_ARRAY(graph.edgePtr, graph.numNodes);
  WARM_UP_ARRAY(graph.edges, graph.numEdges);
  WARM_UP_ARRAY(values, graph.numNodes);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = values[tid];
  }
#endif
#ifdef GEM_FORGE
  m5_reset_stats(0, 0);
#endif

  volatile int ret = foo(&graph, values, 10);
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
