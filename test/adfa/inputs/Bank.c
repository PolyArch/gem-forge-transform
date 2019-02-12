/**
 * A very simple loop body with no loop-carry dependence.
 */

#define N 64

typedef long long T;

T arr[N];
T arr2[N];
T arr3[N];

T foo(volatile T *a) {
  T x = 0;
  for (int i = 0; i < 64; i += 4) {
    volatile T *p = a + i;
    x = *(p + 0);
    x = *(p + 1);
    x = *(p + 2);
    x = *(p + 3);
  }
  return x;
}

int main() {
  T b = foo(arr);
  return 0;
}