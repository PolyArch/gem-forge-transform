// Simple linked list iterate.
#include <stdlib.h>

struct Node {
  struct Node *next;
  float val;
};

__attribute__((noinline)) float foo(struct Node *head) {
  float sum = 0.0f;
  while (head != NULL) {
    sum += head->val;
    head = head->next;
  }
  return sum;
}

int main() {
  const int N = 32;
  struct Node *head = NULL;
  for (int i = 0; i < N; ++i) {
    struct Node *node = malloc(sizeof(struct Node));
    node->next = head;
    head = node;
  }
  volatile float c = foo(head);
  return 0;
}