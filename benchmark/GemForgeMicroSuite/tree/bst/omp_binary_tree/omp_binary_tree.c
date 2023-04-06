#include "gfm_utils.h"

#ifndef NO_OMP
#include <omp.h>
#endif
#include <stdlib.h>

#include "../bst.h"

#define WARM_CACHE

typedef int64_t Value;

void foo_ground_truth(struct BSTreeNode *root, Value *keys, int64_t totalKeys,
                      uint8_t *matched) {
  for (int64_t i = 0; i < totalKeys; ++i) {
    Value key = keys[i];
    struct BSTreeNode *node = root;
    uint8_t found = 0;
    Value val = 0;
    do {
      val = node->val;
      struct BSTreeNode *lhs = node->lhs;
      struct BSTreeNode *rhs = node->rhs;
      node = (val > key) ? lhs : rhs;
      found = found || (val == key);
    } while (val != key && node != NULL);
    matched[i] = found;
  }
}

__attribute__((noinline)) void foo(struct BSTreeNode *root, Value *keys,
                                   int64_t totalKeys, uint8_t *matched) {
#ifndef NO_OMP
#pragma omp parallel for schedule(static) firstprivate(root, keys, matched)
#endif
  for (int64_t i = 0; i < totalKeys; ++i) {
#pragma ss stream_name "gfm.bin_tree.keys.ld"
    Value key = keys[i];
    struct BSTreeNode *node = root;
    uint8_t found = 0;
    Value val = 0;
    do {
#pragma ss stream_name "gfm.bin_tree.val.ld"
      Value v = node->val;
#pragma ss stream_name "gfm.bin_tree.lhs.ld"
      struct BSTreeNode *lhs = node->lhs;
#pragma ss stream_name "gfm.bin_tree.rhs.ld"
      struct BSTreeNode *rhs = node->rhs;
      val = v;
      node = (val > key) ? lhs : rhs;
      found = found || (val == key);
    } while (val != key && node != NULL);
#pragma ss stream_name "gfm.bin_tree.match.st"
    matched[i] = found;
  }
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t totalElements = 512 * 1024;
  uint64_t totalKeys = 8 * 1024 * 1024;
  uint64_t hitRatio = 8;
  int warm = 1;
  int check = 1;
  if (argc >= 2) {
    numThreads = atoi(argv[1]);
  }
  if (argc >= 3) {
    totalElements = atoll(argv[2]);
  }
  if (argc >= 4) {
    totalKeys = atoll(argv[3]);
  }
  if (argc >= 5) {
    hitRatio = atoll(argv[4]);
  }
  if (argc >= 6) {
    check = atoi(argv[5]);
  }
  if (argc >= 7) {
    warm = atoi(argv[6]);
  }

  Value keyMax = totalElements * hitRatio;

  printf("NumThreads %d. TotalElm %lu. Tree %lu kB/Thread.\n", numThreads,
         totalElements,
         totalElements * sizeof(struct BSTreeNode) / numThreads / 1024);
  printf("TotalKeys %lu. HitRatio %lu. KeyMax %lu. Key %lu kB/Thread.\n",
         totalKeys, hitRatio, keyMax,
         totalKeys * sizeof(Value) / numThreads / 1024);

#ifdef USE_AFFINITY_ALLOC
#ifdef RANDOM_BUILD_TREE
  printf("Start to randomly generate tree with affinity alloc.\n");
  struct BSTree tree =
      generateRandomTreeWithAffinityAlloc(keyMax, totalElements);
#else
  printf("Start to generate tree with affinity alloc.\n");
  struct BSTree tree =
      generateUniformTreeWithAffinityAlloc(keyMax, totalElements);
#endif
  printf("Tree generated.\n");
#else
  struct BSTree tree = loadTree(keyMax, totalElements);
  if (!tree.array) {
    tree = generateUniformTree(keyMax, totalElements);
    dumpTree(&tree, keyMax);
    printf("Tree generated.\n");
  }
#endif

#ifndef NO_OMP
  omp_set_dynamic(0);
  omp_set_num_threads(numThreads);
  omp_set_schedule(omp_sched_static, 0);
#endif

  // Generate the keys.
  Value *keys = alignedAllocAndTouch(totalKeys, sizeof(Value));
  for (int64_t i = 0; i < totalKeys; ++i) {
    keys[i] = (int64_t)(((float)(rand()) / (float)(RAND_MAX)) * keyMax);
  }

  // Allocated the matched results.
  uint8_t *matched = alignedAllocAndTouch(totalKeys, sizeof(uint8_t));

  if (tree.array) {
    gf_stream_nuca_region("gfm.bin_tree.tree", tree.array,
                          sizeof(tree.array[0]), totalElements);
  }
  gf_stream_nuca_region("gfm.bin_tree.keys", keys, sizeof(keys[0]), totalKeys);
  gf_stream_nuca_region("gfm.bin_tree.match", matched, sizeof(matched[0]),
                        totalKeys);
  /**
   * Due to the RemoteConfig feature, we want to avoid hotspot for NestRegion.
   * Change this interleave to 64 should help a little bit. Otherwise we have to
   * implement some policy to arbitrate RemoteConfig.
   */
  gf_stream_nuca_set_property(keys, STREAM_NUCA_REGION_PROPERTY_INTERLEAVE, 64);
  gf_stream_nuca_remap();

  gf_detail_sim_start();

  if (warm) {
    gf_warm_array("keys", keys, totalKeys * sizeof(keys[0]));
    gf_warm_array("match", matched, totalKeys * sizeof(matched[0]));
    if (tree.array != NULL) {
      gf_warm_array("tree", tree.array, totalElements * sizeof(tree.array[0]));
    } else {
      printf("Start to warm the tree.\n");
      volatile Value v = warmBST(tree.root);
      printf("Warmed the tree.\n");
    }
  }
  startThreads(numThreads);

  gf_reset_stats();
  foo(tree.root, keys, totalKeys, matched);
  gf_detail_sim_end();

  if (check) {
    uint8_t *expected_matched = aligned_alloc(64, sizeof(uint8_t) * totalKeys);
    foo_ground_truth(tree.root, keys, totalKeys, expected_matched);
    uint64_t totalHits = 0;
    for (int64_t i = 0; i < totalKeys; ++i) {
      Value key = keys[i];
      uint8_t expected = expected_matched[i];
      uint8_t found = matched[i];
      if (found) {
        totalHits++;
      }
      if (found != expected) {
        printf("Mismatch %ldth Key = %ld VAddr %p. Expected %d != Ret %d. "
               "TotalElements "
               "%lu.\n",
               i, key, matched + i, expected, found, totalElements);
        gf_panic();
      }
    }
    printf("All matched. TotalHits %lu.\n", totalHits);
  } else {
    printf("No check.\n");
  }
  return 0;
}
