/**
 * Simple synchronious bfs.
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
#include "../gfm_utils.h"

GraphCSR graph;

// Value for computation.
typedef struct {
  uint32_t dist;
} Value;
Value *values;
Value *next_values;

#define CHECK
#define WARM_CACHE

#define INF_DIST (UINT32_MAX - 1)

__attribute__((noinline)) int foo(GraphCSR *graph, Value *vs, Value *next_vs,
                                  int iters) {

  int iter = 0;

  const GraphIndexT numNodes = graph->numNodes;
  const GraphIndexT *nodes = graph->edgePtr;
  const GraphIndexT *edges = graph->edges;

  for (iter = 0; iter < iters; ++iter) {
#ifdef GEM_FORGE
    m5_work_begin(0, 0);
#else
    printf("Iters %d.\n", iter);
#endif

#pragma omp parallel for schedule(static)                                      \
    firstprivate(numNodes, nodes, edges, vs, next_vs, iter)
    for (uint64_t i = 0; i < numNodes; ++i) {
      uint64_t start = nodes[i];
      uint64_t end = nodes[i + 1];
      uint32_t dist = vs[i].dist + 1;
      if (dist == iter + 1) {
#pragma clang loop unroll(disable) interleave(disable)
        for (uint64_t j = start; j < end; ++j) {
          uint64_t n = edges[j];
          __atomic_fetch_min(&next_vs[n].dist, dist, __ATOMIC_RELAXED);
        }
      }
    }

#ifdef GEM_FORGE
    m5_work_end(0, 0);
    m5_work_begin(1, 0);
#endif

    int global_changed = 0;
#pragma omp parallel
    {
      int local_changed = 0;
      // Disable kernel2 for now.

#pragma omp for schedule(static)                                               \
    firstprivate(numNodes, nodes, edges, vs, next_vs)
      for (uint64_t i = 0; i < numNodes; ++i) {
        uint32_t dist = vs[i].dist;
        uint32_t next_dist = next_vs[i].dist;
        vs[i].dist = next_dist;
        local_changed = local_changed | (dist != next_dist);
      }

      __atomic_fetch_or(&global_changed, local_changed, __ATOMIC_RELAXED);
    }

#ifdef GEM_FORGE
    m5_work_end(1, 0);
#endif

    if (!global_changed) {
      break;
    }
  }

  printf("BFS done, iterations %d.\n", iter + 1);
  return iter;
}

// Initialize graph.
void initializeGraph(const char *fn) {
  graph = readInGraph(fn);
  values = aligned_alloc(sizeof(Value), sizeof(Value) * graph.numNodes);
  next_values = aligned_alloc(sizeof(Value), sizeof(Value) * graph.numNodes);
  for (uint64_t i = 0; i < graph.numNodes; ++i) {
    values[i].dist = INF_DIST;
    next_values[i].dist = INF_DIST;
  }
  // Pick up the root for bfs at the middle.
  GraphIndexT root = graph.root;
  values[root].dist = 0;
  next_values[root].dist = 0;
  printf("Value %luMB.\n", sizeof(Value) * graph.numNodes / 1024 / 1024 * 2);
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
  WARM_UP_ARRAY(next_values, graph.numNodes);
  // Start the threads.
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = values[tid];
  }
#endif
#ifdef GEM_FORGE
  m5_reset_stats(0, 0);
#endif

  volatile int ret = foo(&graph, values, next_values, 10000000);
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
