// Simple dense vector dot product.
typedef double Value;

__attribute__((noinline)) Value foo(Value *a, Value *b, Value *c, int N) {
  // Make sure there is no reuse.
  for (int i = 0; i < N; i += 1) {
    c[i] = a[i] + b[i];
  }
  // Unroll by 2 to check coalesce.
  // for (int i = 0; i < N; i += 4) {
  //   c += a[i] * b[i];
  //   c += a[i + 1] * b[i + 1];
  //   c += a[i + 2] * b[i + 2];
  //   c += a[i + 3] * b[i + 3];
  // }
  return c[0];
}

// 65536*4 is 512kB.
const int N = 1080*1920;
Value a[N];
Value b[N];
Value c[N];

int main() {
  volatile Value ret;
  ret = foo(a, b, c, N);
  return 0;
}
