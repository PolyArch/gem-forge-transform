// Simple dense vector dot product.

__attribute__((noinline)) float foo(float *a, float *b, int N) {
  float c = 0.0f;
  for (int i = 0; i < N; ++i) {
    c += a[i] * b[i];
  }
  return c;
}

int main() {
  const int N = 32;
  float a[N];
  float b[N];
  volatile float c;
  c = foo(a, b, N);
  return 0;
}