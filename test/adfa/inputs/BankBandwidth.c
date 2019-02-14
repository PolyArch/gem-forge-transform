/**
 * A very simple loop body with no loop-carry dependence.
 */

// Make this L2 workset 256kB.
#define N (1024 * 32)

typedef long long T;

T arr[N];
T arr2[N];
T arr3[N];

T foo(volatile T *a) {
  T x = 0;
  // Unroll by 4.
  for (int i = 0; i < N; i += 4) {
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