// Simple pointer indirect iteration with back edge dependence.
#include <stdlib.h>

struct Node {
  struct Node *next;
  float val;
  int index;
};

__attribute__((noinline)) float foo(struct Node *head, float *data) {
  float sum = 0.0f;
  while (head->index != -1) {
    sum += head->val * data[head->index];
    head++;
  }
  return sum;
}

#define N 65536

struct Node nodes[N];
float data[N];

int main() {
  for (int i = 0; i < N; ++i) {
    nodes[i].index = rand() % N;
  }
  // Terminate condition.
  nodes[N - 1].index = -1;
  volatile float c = foo(nodes, data);
  return 0;
}