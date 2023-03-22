// Search in link list.
#include "gfm_utils.h"

#include <omp.h>
#include <stdlib.h>

#ifndef BULK_SIZE
#define BULK_SIZE 1
#endif

typedef int64_t Value;

struct Node {
  struct Node *next;
  Value val;
  // Make this one cache line.
  uint64_t dummy[6];
};

__attribute__((noinline)) Value foo_warm(struct Node **heads,
                                         int64_t numLists) {
  Value ret = 0;
  for (int64_t i = 0; i < numLists; ++i) {
    struct Node *head = heads[i];
    do {
      ret += head->val;
      head = head->next;
    } while (head != NULL);
  }
  return ret;
}

__attribute__((noinline)) Value foo(struct Node **heads, int64_t numLists,
                                    Value terminateValue) {

  Value ret = 0;
#pragma omp parallel
  {
    Value localRet = 0;
#pragma omp for nowait schedule(static) firstprivate(heads, terminateValue)
    for (int64_t i = 0; i < numLists; ++i) {
      struct Node *head = heads[i];
      Value val = 0;
      Value sum = 0;
      /**
       * This helps us getting a single BB loop body, with simple condition.
       * We assert that head != null so that outer stream "localRet" can
       * always get the value on stream "sum".
       */
      do {
#pragma ss stream_name "gfm.link_list.val.ld"
        Value v = head->val;

#pragma ss stream_name "gfm.link_list.next.ld"
        struct Node *next = head->next;

        val = v;
        head = next;
        sum += val;
      } while (val != terminateValue && head != NULL);

      localRet += sum;
    }

    __atomic_fetch_add(&ret, localRet, __ATOMIC_RELAXED);
  }
  return ret;
}

struct InputArgs {
  int numThreads;
  uint64_t elementsPerList;
  uint64_t numLists;
  int check;
  int warm;
};

struct InputArgs parseArgs(int argc, char *argv[]) {

  struct InputArgs args;
  args.numThreads = 1;
  args.elementsPerList = 32 * 1024;
  args.numLists = 1;
  args.check = 0;
  args.warm = 0;

  int argx = 2;
  if (argc >= argx) {
    args.numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    args.elementsPerList = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    args.numLists = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    args.check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    args.warm = atoi(argv[argx - 1]);
  }
  argx++;

  return args;
}

const int MAX_FILE_NAME = 256;
char fileName[MAX_FILE_NAME];

const char *generateFileName(struct InputArgs args) {

  uint64_t totalElementBytes =
      args.numLists * args.elementsPerList * sizeof(struct Node);
  const char *elementBytesSuffix = formatBytes(&totalElementBytes);

  snprintf(fileName, MAX_FILE_NAME, "../link_list_search_%lu%s_%lu_%lu.data",
           totalElementBytes, elementBytesSuffix, args.numLists,
           args.elementsPerList);

  return fileName;
}

struct DataArrays {
  struct Node *nodes;
  struct Node **heads;
};

void dumpData(struct DataArrays data, struct InputArgs args,
              const char *fileName) {
  FILE *f = fopen(fileName, "wb");
  if (!f) {
    gf_panic();
  }
  /**
   * Format:
   * 1. Address of nodes.
   * 2. binary dumping each array.
   */
  fwrite(&data.nodes, sizeof(data.nodes), 1, f);
  fwrite(data.nodes, sizeof(struct Node), args.elementsPerList * args.numLists,
         f);
  fwrite(data.heads, sizeof(struct Node *), args.numLists, f);
  fclose(f);
}

struct DataArrays loadData(struct InputArgs args, const char *fileName) {
  struct DataArrays data;
  data.nodes = NULL;
  data.heads = NULL;

  FILE *f = fopen(fileName, "rb");
  if (f) {

    struct Node *oldNodesPtr = NULL;
    fread(&oldNodesPtr, sizeof(oldNodesPtr), 1, f);
    uint64_t numNodes = args.elementsPerList * args.numLists;

    data.nodes = aligned_alloc(PAGE_SIZE, sizeof(struct Node) * numNodes);
    data.heads =
        aligned_alloc(PAGE_SIZE, sizeof(struct Node *) * args.numLists);

    for (uint64_t i = 0; i < numNodes; ++i) {
      data.nodes[i].next = NULL;
    }
    for (uint64_t i = 0; i < args.numLists; ++i) {
      data.heads[i] = NULL;
    }

    fread(data.nodes, sizeof(struct Node), numNodes, f);
    fread(data.heads, sizeof(struct Node *), args.numLists, f);

    /**
     * Fix all the pointers.
     */
    for (uint64_t i = 0; i < numNodes; ++i) {
      if (data.nodes[i].next != NULL) {
        data.nodes[i].next = data.nodes + (data.nodes[i].next - oldNodesPtr);
      }
    }
    for (uint64_t i = 0; i < args.numLists; ++i) {
      if (data.heads[i] != NULL) {
        data.heads[i] = data.nodes + (data.heads[i] - oldNodesPtr);
      }
    }
    printf("Loaded data from file %s.\n", fileName);

    fclose(f);
  }

  return data;
}

struct DataArrays generateDataWithAffinityAlloc(struct InputArgs args) {
  uint64_t totalNodes = args.elementsPerList * args.numLists;
  uint64_t totalBulks = totalNodes / BULK_SIZE;

  printf("Start to generate data with affinity allocator.\n");
  struct Node **heads =
      aligned_alloc(PAGE_SIZE, sizeof(struct Node *) * args.numLists);

  // Create and connect the nodes.
  for (int64_t j = 0; j < args.numLists; ++j) {
    struct Node *node = malloc_aff(sizeof(struct Node), 0, NULL);
    Value baseValue = j * args.elementsPerList;
    node->val = baseValue;
    node->next = NULL;
    for (int64_t i = 1; i < args.elementsPerList; ++i) {
      struct Node *prevNode = node;
      const void *ptr = prevNode;
      node = malloc_aff(sizeof(struct Node), 1, &ptr);
      node->val = baseValue + i;
      node->next = prevNode;
    }
    heads[j] = node;
  }

  struct DataArrays data;
  data.nodes = NULL;
  data.heads = heads;

  return data;
}

struct DataArrays generateData(struct InputArgs args) {

#ifdef USE_AFFINITY_ALLOC
  return generateDataWithAffinityAlloc(args);
#endif

  uint64_t totalNodes = args.elementsPerList * args.numLists;
  uint64_t totalBulks = totalNodes / BULK_SIZE;
  const char *fileName = generateFileName(args);

  {
    struct DataArrays data = loadData(args, fileName);
    if (data.nodes != NULL) {
      return data;
    }
  }

  printf("Start to generate data.\n");
  struct Node *nodes =
      aligned_alloc(PAGE_SIZE, sizeof(struct Node) * totalNodes);
  struct Node **nodes_ptr =
      aligned_alloc(PAGE_SIZE, sizeof(struct Node *) * totalBulks);
  struct Node **heads =
      aligned_alloc(PAGE_SIZE, sizeof(struct Node *) * args.numLists);

  for (uint64_t i = 0; i < totalNodes; ++i) {
    nodes[i].next = NULL;
    nodes[i].val = i;
  }
  for (uint64_t i = 0; i < args.numLists; ++i) {
    heads[i] = NULL;
  }

  for (uint64_t i = 0; i < totalBulks; ++i) {
    nodes_ptr[i] = &nodes[i * BULK_SIZE];
  }

  // Shuffle the nodes.
  for (int j = totalBulks - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    struct Node *tmp = nodes_ptr[i];
    nodes_ptr[i] = nodes_ptr[j];
    nodes_ptr[j] = tmp;
  }

// Connect the nodes.
#define GET_PTR(nodeId) (nodes_ptr[(nodeId) / BULK_SIZE] + (nodeId) % BULK_SIZE)
  for (int64_t j = 0; j < args.numLists; ++j) {
    int64_t listStartNodeId = j * args.elementsPerList;
    heads[j] = GET_PTR(listStartNodeId);
    for (int64_t i = 0; i < args.elementsPerList - 1; ++i) {
      int64_t idx = j * args.elementsPerList + i;
      GET_PTR(idx)->next = GET_PTR(idx + 1);
      // printf("%ld %ld %ld.\n", j, i, GET_PTR(idx)->next - nodes);
    }
  }

  struct DataArrays data;
  data.nodes = nodes;
  data.heads = heads;

  dumpData(data, args, fileName);

  return data;
}

int main(int argc, char *argv[]) {

  struct InputArgs args = parseArgs(argc, argv);

  printf("NumThreads %d. numLists %lu. N %lu. %lu kB/Thread.\n",
         args.numThreads, args.numLists, args.elementsPerList,
         args.elementsPerList * args.numLists * sizeof(struct Node) / 1024);

  omp_set_dynamic(0);
  omp_set_num_threads(args.numThreads);
  omp_set_schedule(omp_sched_static, 0);

  /**
   * Interesting fact: creating the linked list via
   * a sequence of malloc of the same size will actually
   * result in a lot of locality. The difference between
   * the consecutive allocated addresses is always 32 in
   * this example.
   */
  struct DataArrays data = generateData(args);

  // Make sure we never break out with a large value.
  uint64_t totalNodes = args.elementsPerList * args.numLists;
  Value terminateValue = totalNodes;

  gf_stream_nuca_region("gfm.link_list.heads", data.heads,
                        sizeof(data.heads[0]), args.numLists);
  if (data.nodes != NULL) {
    gf_stream_nuca_region("gfm.link_list.nodes", data.nodes,
                          sizeof(data.nodes[0]), totalNodes);
    const int64_t ptrOffset = (int64_t)(&(((struct Node *)0)->next));
    const int64_t ptrSize = sizeof(((struct Node *)0)->next);
    gf_stream_nuca_align(data.nodes, data.heads,
                         m5_stream_nuca_encode_ptr_align(ptrOffset, ptrSize));
  }
  gf_stream_nuca_remap();

  gf_detail_sim_start();

  if (args.warm) {
    gf_warm_array("gfm.link_list.heads", data.heads,
                  args.numLists * sizeof(data.heads[0]));
    if (data.nodes != NULL) {
      gf_warm_array("gfm.link_list.nodes", data.nodes,
                    totalNodes * sizeof(struct Node));
    } else {
      printf("Start to warm link list.\n");
      volatile Value ret = foo_warm(data.heads, args.numLists);
      printf("Warmed link list.\n");
    }
  }

  startThreads(args.numThreads);

  gf_reset_stats();
  volatile Value ret = foo(data.heads, args.numLists, terminateValue);
  gf_detail_sim_end();

  if (args.check) {
    Value expected = totalNodes * (totalNodes - 1) / 2;
    printf("Ret = %ld, Expected = %ld.\n", ret, expected);
    if (expected != ret) {
      gf_panic();
    }
  }
  return 0;
}