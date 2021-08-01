// Search in link list.
#include "gfm_utils.h"

#include <omp.h>
#include <stdlib.h>

typedef int64_t Value;

#define WARM_CACHE

struct Node {
  struct Node *next;
  Value val;
};

__attribute__((noinline)) void foo(struct Node **heads, uint64_t hashMask,
                                   Value *keys, int64_t totalKeys,
                                   uint8_t *matched) {

#pragma omp parallel for schedule(static)                                      \
    firstprivate(heads, hashMask, keys, matched)
  for (int64_t i = 0; i < totalKeys; ++i) {
    Value key = keys[i];
    Value hash = key & hashMask;
    struct Node *head = heads[hash];
    Value val = 0;
    uint8_t found = 0;
    if (head) {
      /**
       * This helps us getting a single BB loop body, with simple condition.
       */
      do {
        val = head->val;
        head = head->next;
        // This ensures that found is a reduction variable.
        found = found || (val == key);
      } while (val != key && head != NULL);
    }
    matched[i] = found;
  }
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t totalElements = 512 * 1024;
  uint64_t elementsPerBucket = 8;
  uint64_t totalKeys = 8 * 1024 * 1024;
  uint64_t hitRatio = 8;
  int check = 1;
  if (argc >= 2) {
    numThreads = atoi(argv[1]);
  }
  if (argc >= 3) {
    totalElements = atoll(argv[2]);
  }
  if (argc >= 4) {
    elementsPerBucket = atoll(argv[3]);
  }
  if (argc >= 5) {
    totalKeys = atoll(argv[4]);
  }
  if (argc >= 6) {
    hitRatio = atoll(argv[5]);
  }
  if (argc >= 7) {
    check = atoi(argv[6]);
  }

  const uint64_t totalBuckets = totalElements / elementsPerBucket;
  if (totalBuckets & (totalBuckets - 1)) {
    printf("TotalBucket %lu must be power of 2.\n", totalBuckets);
    gf_panic();
  }
  const uint64_t hashMask = totalBuckets - 1;
  printf("NumThreads %d. TotalElm %lu. ElmPerBkt %lu. NumBkt %lu. Elm %lu "
         "kB/Thread.\n",
         numThreads, totalElements, elementsPerBucket, totalBuckets,
         totalElements * sizeof(struct Node) / numThreads / 1024);
  printf("TotalKeys %lu. HitRatio %lu. Key %lu kB/Thread.\n", totalKeys,
         hitRatio, totalKeys * sizeof(Value) / numThreads / 1024);

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
  uint64_t totalNodes = totalElements;
  struct Node *nodes = aligned_alloc(64, sizeof(struct Node) * totalNodes);
  struct Node **nodes_ptr =
      aligned_alloc(64, sizeof(struct Node *) * totalNodes);
  struct Node **heads = aligned_alloc(64, sizeof(struct Node *) * totalBuckets);

  for (uint64_t i = 0; i < totalNodes; ++i) {
    nodes[i].next = NULL;
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
  for (int64_t j = 0; j < totalBuckets; ++j) {
    heads[j] = nodes_ptr[j * elementsPerBucket];
    for (int64_t i = 0; i < elementsPerBucket; ++i) {
      int64_t idx = j * elementsPerBucket + i;
      if (i < elementsPerBucket - 1) {
        // Connect.
        nodes_ptr[idx]->next = nodes_ptr[idx + 1];
      }
      // Set one value with the correct hash values.
      Value val = i * totalBuckets + j;
      nodes_ptr[idx]->val = val;
    }
  }

  // Generate the keys.
  Value *keys = aligned_alloc(64, sizeof(Value) * totalKeys);
  Value keyMax = totalElements * hitRatio;
  for (int64_t i = 0; i < totalKeys; ++i) {
    keys[i] = (int64_t)(((float)(rand()) / (float)(RAND_MAX)) * keyMax);
  }

  // Allocated the matched results.
  uint8_t *matched = aligned_alloc(64, sizeof(uint8_t) * totalKeys);

  gf_detail_sim_start();

#ifdef WARM_CACHE
  WARM_UP_ARRAY(nodes, totalNodes);
  WARM_UP_ARRAY(keys, totalKeys);
  WARM_UP_ARRAY(matched, totalKeys);
#pragma omp parallel for schedule(static)
  for (int tid = 0; tid < numThreads; ++tid) {
    volatile Value x = nodes[tid].val;
  }
#endif

  gf_reset_stats();
  foo(heads, hashMask, keys, totalKeys, matched);
  gf_detail_sim_end();

  if (check) {
    for (int64_t i = 0; i < totalKeys; ++i) {
      Value key = keys[i];
      uint8_t expected = (key < totalElements);
      uint8_t found = matched[i];
      if (found != expected) {
        printf("Mismatch %ldth Key = %ld. Expected %d != Ret %d. TotalElements "
               "%lu.\n",
               i, key, expected, found, totalElements);
        gf_panic();
      }
    }
    printf("All matched.\n");
  } else {
    printf("No check.\n");
  }
  return 0;
}