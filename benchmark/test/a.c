
#include <stdio.h>
#include <stdlib.h>

int arr[1024];
int dst[1024];

void dump(int *a);

struct ListNode {
  int Value;
  struct ListNode *Next;
};

int foo_recursive(int i) {
  if (i == 1) {
    return 1;
  } else {
    return foo_recursive(i - 1) + rand();
  }
}

int main(int argc, char *argv[]) {
  // for (int i = 0; i < 1024; ++i) {
  //   arr[i] = rand();
  // }
  // for (int i = 0; i < 10; ++i) {
  //   arr[i % 5] += rand();
  // }
  // int i = 0;
  // while (i < 100) {
  //   if (arr[i] % 3 == 2) {
  //   } else {
  //     arr[i]++;
  //   }
  //   i++;
  // }
  // dump(arr);

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

  // Test with loop status.
  // This is INCONTINUOUS.
  // for (int i = 0; i < 100; ++i) {
  //   printf("%d, ", i);
  // }

  // This one should be INLINE_CONTINUOUS.
  for (int i = 0; i < 100; ++i) {
    dump(&i);
  }

  // // CONTINUOUS.
  // for (int i = 0; i < 100; ++i) {
  //   arr[i] = rand();
  // }
  // dump(arr);

  // // Recursive.
  // for (int i = 1; i < 10; ++i) {
  //   foo_recursive(i);
  // }

  return 0;
}
