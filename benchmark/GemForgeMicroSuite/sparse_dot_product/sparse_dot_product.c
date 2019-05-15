#include <stdlib.h>

__attribute__((noinline)) float foo(float *a, float *b, int *ia, int *ib,
                                    int N) {
  int x = 0;
  int y = 0;
  float c = 0;
  while (x < N && y < N) {
    if (ia[x] == ib[y]) {
      c += a[x] * b[y];
      x++;
      y++;
    } else if (ia[x] < ib[y]) {
      x++;
    } else {
      y++;
    }
  }
  return c;
}
const int N = 5;
float a[N];
float b[N];

int main() {
  int ia[] = {0, 2, 4, 6, 8};
  int ib[] = {1, 2, 3, 8, 9};
  volatile float c;
  c = foo(a, b, ia, ib, N);
  return 0;
}
