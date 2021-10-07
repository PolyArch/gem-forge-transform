#define N 100
#define M 2

int foo(int *a, int *b) {
  int sum = 0;
  for (int i = 1; i < 4; ++i) {
    sum += a[i] - a[i - 1];
    sum += a[i + N] - a[i - 1 + N];
  }
  *b = sum;
  return sum;
}

int sum;
int a[M][N];

int main() {
  foo(a, &sum);
  return 0;
}