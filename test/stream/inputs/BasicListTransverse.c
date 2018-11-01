#define M 2
#define N 3
struct Node {
  int val;
  struct Node *next;
};

void foo(struct Node *head, struct Node *count) {
  for (struct Node *y = count; y; y = y->next) {
    for (struct Node *x = head; x; x = x->next) {
      y->val = x->val;
    }
  }
}

int main() {
  struct Node head;
  struct Node *x = &head;
  for (int i = 0; i < N; ++i) {
    x->next = (struct Node *)malloc(sizeof(struct Node));
    x = x->next;
  }
  x->next = 0;

  struct Node count;
  struct Node *y = &count;
  for (int i = 0; i < M; ++i) {
    y->next = (struct Node *)malloc(sizeof(struct Node));
    y = y->next;
  }
  y->next = 0;
  foo(&head, &count);
  return 0;
}