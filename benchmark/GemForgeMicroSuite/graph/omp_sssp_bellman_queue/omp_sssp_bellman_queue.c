/**
 * Bellman-Ford with queue.
 * ! This won't work so far as the queue explodes.
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

// Value for computation.
typedef struct {
  uint32_t dist;
} Value;
#define INF_DIST (UINT32_MAX - 1)

__attribute__((noinline)) int foo(WeightGraphCSR *graph, Value *vs,
                                  GraphIndexT *global_queue0,
                                  GraphIndexT *global_queue1,
                                  GraphIndexT **local_queues, int iters) {

  int iter = 0;

  const GraphIndexT *nodes = graph->edgePtr;
  const WeightEdgeT *edges = graph->edges;
  const uint64_t numNodes = graph->numNodes;

  uint64_t global_queue_cur_size = 1;
  uint64_t global_queue_nxt_size = 0;

#pragma omp parallel shared(global_queue_cur_size, global_queue_nxt_size,      \
                            iter)                                              \
    firstprivate(nodes, edges, numNodes, vs, global_queue0, global_queue1)
  {
    // Create the local buffer.
    int tid = omp_get_thread_num();
    GraphIndexT *local_queue = local_queues[tid];

    while (global_queue_cur_size > 0) {

#pragma omp single
      {
#ifdef GEM_FORGE
        m5_work_begin(0, 0);
#else
        printf("Iters %d start with size %lu.\n", iter, global_queue_cur_size);
#endif
      }

      uint64_t lhs = 0;
      uint64_t rhs = global_queue_cur_size;
      uint64_t queue_size = 0;

      // Ping pong buffer?
      GraphIndexT *global_queue_cur =
          (iter & 1) ? global_queue1 : global_queue0;
      GraphIndexT *global_queue_nxt =
          (iter & 1) ? global_queue0 : global_queue1;

#pragma omp barrier
#pragma omp for schedule(static)
      for (uint64_t i = lhs; i < rhs; ++i) {
        uint64_t n = global_queue_cur[i];
        uint64_t start = nodes[n];
        uint64_t end = nodes[n + 1];
        uint32_t dist = vs[n].dist;
#pragma clang loop unroll(disable) interleave(disable)
        for (uint64_t j = start; j < end; ++j) {
          uint64_t m = edges[j].destVertex;
          uint32_t inf_dist = INF_DIST;
          WeightT new_dist = dist + edges[j].weight;
          // WeightT new_dist = dist + 1;
          WeightT old_dist =
              __atomic_fetch_min(&vs[m].dist, new_dist, __ATOMIC_RELAXED);
          if (old_dist > new_dist) {
#ifndef GEM_FORGE
            // Well this implementation will likely blow up the queue.
            assert(queue_size < numNodes);
#endif
            local_queue[queue_size] = m;
            queue_size++;
          }
        }
      }

      // Write to global queue.
      const uint64_t nxt_lhs = __atomic_fetch_add(&global_queue_nxt_size,
                                                  queue_size, __ATOMIC_RELAXED);
#ifndef GEM_FORGE
      // Well this implementation will likely blow up the queue.
      assert(nxt_lhs + queue_size < numNodes);
#endif
      __builtin_memcpy(&global_queue_nxt[nxt_lhs], local_queue,
                       queue_size * sizeof(local_queue[0]));

#pragma omp single
      {
        // Switch the global queue.
        global_queue_cur_size = global_queue_nxt_size;
        global_queue_nxt_size = 0;
#ifdef GEM_FORGE
        m5_work_end(0, 0);
#else
        printf("Iters %d done.\n", iter);
#endif
        iter++;
      }

// Barrier.
#pragma omp barrier
    }
  }

  printf("SSSP done, iterations %d.\n", iter);
  return iter;
}

WeightGraphCSR graph;
Value *values;

#define CHECK
#define WARM_CACHE

// Initialize graph.
void initializeGraph(const char *fn) {
  graph = readInWeightGraph(fn);
  values = aligned_alloc(sizeof(Value), sizeof(Value) * graph.numNodes);
  for (uint64_t i = 0; i < graph.numNodes; ++i) {
    values[i].dist = INF_DIST;
  }
  // Pick up the root for bfs at the middle.
  GraphIndexT root = graph.root;
  values[root].dist = 0;
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

  // Aggressively allocate all the queues.
  GraphIndexT *global_queue0 =
      malloc(sizeof(global_queue0[0]) * graph.numNodes);
  GraphIndexT *global_queue1 =
      malloc(sizeof(global_queue1[0]) * graph.numNodes);
  GraphIndexT **local_queues = malloc(sizeof(local_queues[0]) * numThreads);
  for (int tid = 0; tid < numThreads; ++tid) {
    local_queues[tid] =
        aligned_alloc(CACHE_LINE_SIZE, sizeof(GraphIndexT) * graph.numNodes);
  }
  global_queue0[0] = graph.root;

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
#else
  clock_t start_time = clock();
#endif

  volatile int ret =
      foo(&graph, values, global_queue0, global_queue1, local_queues, 10000000);
#ifdef GEM_FORGE
  m5_detail_sim_end();
  exit(0);
#else
  clock_t end_time = clock();
  printf("Time: %fms.\n",
         (1000.0 * (double)(end_time - start_time)) / CLOCKS_PER_SEC);
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
