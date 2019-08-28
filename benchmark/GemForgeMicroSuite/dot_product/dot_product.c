// Simple dense vector dot product.
typedef double Value;

__attribute__((noinline)) Value foo(Value **pa, Value **pb, int N) {
  Value c = 0.0f;
  Value *a = *pa;
  Value *b = *pb;
  // Make sure there is no reuse.
  #pragma nounroll
  for (int i = 0; i < N; i += 1) {
    c += a[i] * b[i];
  }
  // Unroll by 2 to check coalesce.
  // for (int i = 0; i < N; i += 4) {
  //   c += a[i] * b[i];
  //   c += a[i + 1] * b[i + 1];
  //   c += a[i + 2] * b[i + 2];
  //   c += a[i + 3] * b[i + 3];
  // }
  return c;
}

// 65536*4 is 512kB.
const int N = 65536 / 2;
Value a[N];
Value b[N];

int main() {
  volatile Value c;
  Value *pa = a;
  Value *pb = b;
  c = foo(&pa, &pb, N);
  return 0;
}
