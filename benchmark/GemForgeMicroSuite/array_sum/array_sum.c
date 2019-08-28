// Simple dense vector dot product.
#include "../gem5_pseudo.h"
typedef int Value;

__attribute__((noinline)) Value foo(Value **pa, int N) {
  Value sum = 0.0f;
  volatile Value *a = *pa;
  // Make sure there is no reuse.
  #pragma nounroll
  for (long long i = 0; i < N; i += 16) {
    sum += a[i];
  }
  // Unroll by 2 to check coalesce.
  // for (int i = 0; i < N; i += 4) {
  //   c += a[i] * b[i];
  //   c += a[i + 1] * b[i + 1];
  //   c += a[i + 2] * b[i + 2];
  //   c += a[i + 3] * b[i + 3];
  // }
  return sum;
}

// 65536*8 is 512kB.
const int N = 65536 * 2;
Value a[N];

int main() {
  volatile Value c;
  Value *pa = a;
  // This should warm up the cache.
  for (long long i = 0; i < N; i++) {
    a[i] = i;
  }
  DETAILED_SIM_START();
  c = foo(&pa, N);
  DETAILED_SIM_STOP();
  return 0;
}
