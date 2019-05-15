// Simple dense vector dot product.
typedef double Value;

__attribute__((noinline)) Value foo(volatile Value *a, int N) {
  // Make sure there is no reuse.
  #pragma nounroll
  for (int i = 1; i < N; i += 1) {
    a[i] += a[i - 1];
  }
  return a[N - 1];
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];

int main() {
  volatile Value ret;
  ret = foo(a, N);
  return 0;
}
