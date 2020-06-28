/**
 * Delta skipping with global buckets.
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

typedef struct {
  GraphIndexT *data;
  uint64_t size;
} Bucket;

__attribute__((noinline)) int foo(WeightGraphCSR *graph, Value *vs,
                                  Bucket *globalBuckets,
                                  Bucket *globalUpdateBuffer, int numBuckets,
                                  WeightT delta, int iters) {

  int iter = 0;

  const GraphIndexT *nodes = graph->edgePtr;
  const WeightEdgeT *edges = graph->edges;
  const uint64_t numNodes = graph->numNodes;

  // Current working bucket.
  uint64_t currentBucketIdx = 0;
#define getGlobalBucket(idx) (globalBuckets + ((idx) % numBuckets))
#define getBucketIdx(dist) (dist / delta)

#pragma omp parallel shared(currentBucketIdx, iter) firstprivate(              \
    nodes, edges, numNodes, vs, globalBuckets, globalUpdateBuffer)
  {

    while (1) {

      // Get the current global bucket.
      Bucket *currentGlobalBucket = getGlobalBucket(currentBucketIdx);
      uint64_t currentGlobalBucketSize = currentGlobalBucket->size;

      if (currentGlobalBucketSize == 0) {
        break;
      }

#pragma omp single
      {
#ifdef GEM_FORGE
        m5_work_begin(0, 0);
#else
        printf("Iters %d start with bucket %lu size %lu.\n", iter,
               currentBucketIdx, currentGlobalBucketSize);
#endif
      }

#pragma omp barrier
#pragma omp for schedule(static)
      for (uint64_t i = 0; i < currentGlobalBucketSize; ++i) {
        uint64_t n = currentGlobalBucket->data[i];
        uint64_t start = nodes[n];
        uint64_t end = nodes[n + 1];
        uint32_t dist = vs[n].dist;
        // Filter old vertex.
        uint64_t bucketIdx = getBucketIdx(dist);
        if (bucketIdx == currentBucketIdx) {
#pragma clang loop unroll(disable) interleave(disable)
          for (uint64_t j = start; j < end; ++j) {
            uint64_t m = edges[j].destVertex;
            WeightT newDist = dist + edges[j].weight;
            // WeightT new_dist = dist + 1;
            WeightT oldDist =
                __atomic_fetch_min(&vs[m].dist, newDist, __ATOMIC_RELAXED);
            uint64_t newBucketIdx = getBucketIdx(newDist);
            uint64_t oldBucketIdx = getBucketIdx(oldDist);
#ifndef GEM_FORGE
            // Some sanity check.
            assert(newBucketIdx >= currentBucketIdx);
            assert(newBucketIdx < currentBucketIdx + numBuckets);
#endif
            if (newBucketIdx < oldBucketIdx ||
                (newBucketIdx == currentBucketIdx && newDist < oldDist)) {
              Bucket *newBucket = (newBucketIdx == currentBucketIdx)
                                      ? globalUpdateBuffer
                                      : getGlobalBucket(newBucketIdx);
#ifndef GEM_FORGE
              // Well this implementation will likely blow up the queue.
              assert(newBucket->size < numNodes);
#ifndef GEM_FORGE
              // printf("Insert newBucket %lu newDist %u oldDist %u.\n",
              //        newBucketIdx, newDist, oldDist);
#endif
#endif
              // Insert this into the new bucket.
              uint64_t newBucketEnqueueIdx =
                  __atomic_fetch_add(&newBucket->size, 1, __ATOMIC_RELAXED);
              newBucket->data[newBucketEnqueueIdx] = m;
            } else {
#ifndef GEM_FORGE
              // printf("Skip newBucket %lu newDist %u oldDist %u.\n",
              //        newBucketIdx, newDist, oldDist);
#endif
            }
          }
        }
      }

#pragma omp barrier
#pragma omp single
      {
        if (globalUpdateBuffer->size > 0) {
          // We are not done with the current bucket.
          // Switch them and start again.
          Bucket tmp = *globalUpdateBuffer;
          *globalUpdateBuffer = *currentGlobalBucket;
          *currentGlobalBucket = tmp;
        } else {
          // We can move to next bucket.
          currentGlobalBucket->size = 0;
          currentBucketIdx++;
        }
        // Always clear the global update buffer.
        globalUpdateBuffer->size = 0;
#ifdef GEM_FORGE
        m5_work_end(0, 0);
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

  assert(argc == 5);
  int numThreads = atoi(argv[1]);
  printf("Number of Threads: %d.\n", numThreads);

  char *graphFn = argv[2];
  initializeGraph(graphFn);

  uint64_t delta = atoll(argv[3]);
  uint64_t numBuckets = atoll(argv[4]);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  // Aggressively allocate all the queues.
  Bucket *globalBuckets =
      aligned_alloc(CACHE_LINE_SIZE, sizeof(Bucket) * numBuckets);
  for (int i = 0; i < numBuckets; ++i) {
    globalBuckets[i].size = 0;
    globalBuckets[i].data = malloc(sizeof(GraphIndexT) * graph.numNodes);
  }
  // Push the root.
  globalBuckets[0].size = 1;
  globalBuckets[0].data[0] = graph.root;
  // Allocate the globalUpdateBuffer.
  Bucket globalUpdateBuffer;
  globalUpdateBuffer.data = malloc(sizeof(GraphIndexT) * graph.numNodes);
  globalUpdateBuffer.size = 0;

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

  volatile int ret = foo(&graph, values, globalBuckets, &globalUpdateBuffer,
                         numBuckets, delta, 10000000);
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
