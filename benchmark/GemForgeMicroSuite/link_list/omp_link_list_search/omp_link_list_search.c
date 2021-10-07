// Search in link list.
#include "gfm_utils.h"

#include <omp.h>
#include <stdlib.h>

typedef int64_t Value;

#define WARM_CACHE
#define CHECK

struct Node {
  struct Node *next;
  Value val;
};

Value foo_warm(struct Node **heads, int64_t numLists, Value terminateValue) {

  Value sum = 0;
  for (int64_t i = 0; i < numLists; ++i) {
    struct Node *head = heads[i];
    Value val = 0;
    if (head) {
      do {
        val = head->val;
        head = head->next;
        sum += val;
      } while (val != terminateValue && head != NULL);
    }
  }
  return sum;
}

__attribute__((noinline)) Value foo(struct Node **heads, int64_t numLists,
                                    Value terminateValue) {

  Value ret = 0;
#pragma omp parallel for schedule(static) firstprivate(heads, terminateValue)
  for (int64_t i = 0; i < numLists; ++i) {
    struct Node *head = heads[i];
    Value val = 0;
    Value sum = 0;
    if (head) {
      /**
       * This helps us getting a single BB loop body, with simple condition.
       */
      do {
        val = head->val;
        head = head->next;
        sum += val;
      } while (val != terminateValue && head != NULL);
    }
    __atomic_fetch_add(&ret, sum, __ATOMIC_RELAXED);
  }
  return ret;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;

  if (argc >= 2) {
    numThreads = atoi(argv[1]);
  }

  uint64_t N = 32 * 1024;
  if (argc >= 3) {
    N = atoll(argv[2]);
  }

  int numLists = numThreads;
  if (argc >= 4) {
    numLists = atoi(argv[3]);
  }
  printf("NumThreads %d. numLists %d. N %lu. %lu kB/Thread.\n", numThreads,
         numLists, N, N * numLists * sizeof(struct Node) / 1024);

  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);

  /**
   * Interesting fact: creating the linked list via
   * a sequence of malloc of the same size will actually
   * result in a lot of locality. The difference between
   * the consecutive allocated addresses is always 32 in
   * this example.
   */
  uint64_t totalNodes = N * numLists;
  struct Node *nodes = aligned_alloc(64, sizeof(struct Node) * totalNodes);
  struct Node **nodes_ptr =
      aligned_alloc(64, sizeof(struct Node *) * totalNodes);
  struct Node **heads = aligned_alloc(64, sizeof(struct Node *) * numLists);

  for (uint64_t i = 0; i < totalNodes; ++i) {
    nodes[i].next = NULL;
    nodes[i].val = i;
    nodes_ptr[i] = &nodes[i];
  }

  // Shuffle the nodes.
  for (int j = totalNodes - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    struct Node *tmp = nodes_ptr[i];
    nodes_ptr[i] = nodes_ptr[j];
    nodes_ptr[j] = tmp;
  }

  // Connect the nodes.
  for (int64_t j = 0; j < numLists; ++j) {
    heads[j] = nodes_ptr[j * N];
    for (int64_t i = 0; i < N - 1; ++i) {
      int64_t idx = j * N + i;
      nodes_ptr[idx]->next = nodes_ptr[idx + 1];
    }
  }

  // Make sure we never break out with a large value.
  Value terminateValue = totalNodes;

  gf_detail_sim_start();

#ifdef WARM_CACHE
  WARM_UP_ARRAY(nodes, totalNodes);
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = nodes[tid].val;
  }
#endif

  gf_reset_stats();
  volatile Value ret = foo(heads, numLists, terminateValue);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = totalNodes * (totalNodes - 1) / 2;
  printf("Ret = %ld, Expected = %ld.\n", ret, expected);
  if (expected != ret) {
    gf_panic();
  }
#endif
  return 0;
}