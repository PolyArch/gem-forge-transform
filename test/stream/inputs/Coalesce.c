#define N 10

int foo(int *a, int *b) {
  int sum = 0;
  for (int i = 1; i < N; ++i) {
    sum += a[i] - a[i - 1];
  }
  *b = sum;
  return sum;
}

int sum;

int main() {
  int a[N];
  foo(a, &sum);
  return 0;
}