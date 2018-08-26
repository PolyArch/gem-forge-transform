
#include <stdio.h>
#include <stdlib.h>

int arr[1024];
int dst[1024];

void dump(int *a);

struct ListNode {
  int Value;
  struct ListNode* Next;
};

int main(int argc, char *argv[]) {
  for (int i = 0; i < 1024; ++i) {
    arr[i] = rand();
  }
  for (int i = 0; i < 10; ++i) {
    arr[i % 5] += rand();
  }
  int i = 0;
  while (i < 100) {
    if (arr[i] % 3 == 2) {
    } else {
      arr[i]++;
    }
    i++;
  }
  dump(arr);

  // struct ListNode* head = NULL;
  // for (int i = 0; i < 100; ++i) {
  //   struct ListNode* node = malloc(sizeof(struct ListNode));
  //   node->Next = head;
  //   node->Value = rand();
  //   head = node;
  // }

  // struct ListNode* iter = head;
  // int acc = 0;
  // while (iter != NULL) {
  //   acc += iter->Value;
  //   iter = iter->Next;
  // }

  // iter = head;
  // while (iter != NULL) {
  //   struct ListNode* next = iter->Next;
  //   free(iter);
  //   iter = next;
  // }

  

  return 0;
}
