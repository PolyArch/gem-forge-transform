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

// Value for computation.
typedef struct {
  uint32_t dist;
} Value;
#define INF_DIST (UINT32_MAX - 1)

__attribute__((noinline)) int foo(GraphCSR *graph, Value *vs,
                                  GraphIndexT *global_queue,
                                  GraphIndexT **local_queues, int iters) {

  int iter = 0;

  const GraphIndexT *nodes = graph->edgePtr;
  const GraphIndexT *edges = graph->edges;

  uint64_t global_queue_lhs = 0;
  uint64_t global_queue_rhs = 1;

#pragma omp parallel shared(global_queue_lhs, global_queue_rhs, iter)          \
    firstprivate(nodes, edges, vs, global_queue)
  {
    // Create the local buffer.
    int tid = omp_get_thread_num();
    GraphIndexT *local_queue = local_queues[tid];

    while (global_queue_lhs < global_queue_rhs) {

#pragma omp single
      {
#ifdef GEM_FORGE
        m5_work_begin(0, 0);
#else
        printf("Iters %d start with size %lu lhs %lu rhs %lu.\n", iter,
               global_queue_rhs - global_queue_lhs, global_queue_lhs,
               global_queue_rhs);
#endif
      }

      uint64_t lhs = global_queue_lhs;
      uint64_t rhs = global_queue_rhs;
      uint64_t queue_size = 0;

#pragma omp barrier
#pragma omp for schedule(static) 
      for (uint64_t i = lhs; i < rhs; ++i) {
        uint64_t n = global_queue[i];
        uint64_t start = nodes[n];
        uint64_t end = nodes[n + 1];
        uint32_t dist = vs[n].dist + 1;
#pragma clang loop unroll(disable) interleave(disable)
        for (uint64_t j = start; j < end; ++j) {
          uint64_t m = edges[j];
          uint32_t inf_dist = INF_DIST;
          if (__atomic_compare_exchange_n(&vs[m].dist, &inf_dist, dist,
                                          0 /* weak */, __ATOMIC_RELAXED,
                                          __ATOMIC_RELAXED)) {
            local_queue[queue_size] = m;
            queue_size++;
          }
        }
      }

      // Write to global queue.
      const uint64_t cur_rhs =
          __atomic_fetch_add(&global_queue_rhs, queue_size, __ATOMIC_RELAXED);
      __builtin_memcpy(&global_queue[cur_rhs], local_queue,
                       queue_size * sizeof(local_queue[0]));

#pragma omp single
      {
        global_queue_lhs = rhs;
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

  printf("BFS done, iterations %d.\n", iter);
  return iter;
}

GraphCSR graph;
Value *values;

#define CHECK
#define WARM_CACHE

// Initialize graph.
void initializeGraph(const char *fn) {
  graph = readInGraph(fn);
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
  GraphIndexT *global_queue = malloc(sizeof(global_queue[0]) * graph.numNodes);
  GraphIndexT **local_queues = malloc(sizeof(local_queues[0]) * numThreads);
  for (int tid = 0; tid < numThreads; ++tid) {
    local_queues[tid] =
        aligned_alloc(CACHE_LINE_SIZE, sizeof(GraphIndexT) * graph.numNodes);
  }
  global_queue[0] = graph.root;

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

  volatile int ret = foo(&graph, values, global_queue, local_queues, 10000000);
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
