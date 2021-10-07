// Simple linked list iterate.
#include "gfm_utils.h"
#include <stdlib.h>

typedef int64_t Value;

#define WARM_CACHE
#define CHECK

struct Node {
  struct Node *next;
  Value val;
};

__attribute__((noinline)) Value foo(struct Node *head) {
  Value sum = 0;
  while (head != NULL) {
    sum += head->val;
    head = head->next;
  }
  return sum;
}

const uint64_t N = 65536;

int main() {
  /**
   * Interesting fact: creating the linked list via
   * a sequence of malloc of the same size will actually
   * result in a lot of locality. The difference between
   * the consecutive allocated addresses is always 32 in
   * this example.
   */
  struct Node *nodes = aligned_alloc(64, sizeof(struct Node) * N);
  struct Node **nodes_ptr = aligned_alloc(64, sizeof(struct Node *) * N);
  for (int i = 0; i < N; ++i) {
    nodes[i].next = NULL;
    nodes[i].val = i;
    nodes_ptr[i] = &nodes[i];
  }

  // Shuffle the nodes.
  for (int j = N - 1; j > 0; --j) {
    int i = (int)(((float)(rand()) / (float)(RAND_MAX)) * j);
    struct Node *tmp = nodes_ptr[i];
    nodes_ptr[i] = nodes_ptr[j];
    nodes_ptr[j] = tmp;
  }

  // Connect the nodes.
  for (int i = 0; i < N - 1; ++i) {
    nodes_ptr[i]->next = nodes_ptr[i + 1];
  }

  struct Node *head = nodes_ptr[0];

  gf_detail_sim_start();

#ifdef WARM_CACHE
  WARM_UP_ARRAY(nodes, N);
#endif

  gf_reset_stats();
  volatile Value ret = foo(head);
  gf_detail_sim_end();

#ifdef CHECK
  Value expected = N * (N - 1) / 2;
  printf("Ret = %ld, Expected = %ld.\n", ret, expected);
  if (expected != ret) {
    gf_panic();
  }
#endif
  return 0;
}