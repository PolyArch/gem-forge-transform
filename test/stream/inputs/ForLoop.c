#define M 10

int arr[M];

int foo(volatile int *A) {
  int x = 0;
  for (int i = 0; i < M; ++i) {
    x = A[i];
  }
  return x;
}

int main() {
  int x = foo(arr);
  return 0;
}