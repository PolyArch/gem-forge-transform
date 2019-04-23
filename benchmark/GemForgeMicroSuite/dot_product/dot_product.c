// Simple dense vector dot product.

__attribute__((noinline)) float foo(float *a, float *b, int N) {
  float c = 0.0f;
  for (int i = 0; i < N; ++i) {
    c += a[i] * b[i];
  }
  // Unroll by 2 to check coalesce.
  // for (int i = 0; i < N; i += 2) {
  //   c += a[i] * b[i];
  //   c += a[i + 1] * b[i + 1];
  // }
  return c;
}

int main() {
  // 65536*4 is 256kB.
  const int N = 65536 * 2;
  float a[N];
  float b[N];
  volatile float c;
  c = foo(a, b, N);
  return 0;
}
