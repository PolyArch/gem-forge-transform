#include <stdio.h>

const int N = 0x1000;

int a[N];

void foo(int* a, int v) {
  for (int i = 0; i < N; ++i) {
    a[i] = v + i;
  }
}

int main(int argc, char* argv[]) {
  foo(a, argc);
  printf("%d\n", a[N / 2]);
  // for (int i = 0; i < N; ++i) {
  //   printf("%d\n", a[i]);
  // }
  return 0;
}