#define M 16

int arr[M];

int foo(volatile int **pA) {
  int x = 0;
  volatile int *A = *pA;
  for (int i = 0; i < M; ++i) {
    x = A[i];
  }
  return x;
}

int main() {
  int *A = arr;
  int x = foo(&A);
  return 0;
}