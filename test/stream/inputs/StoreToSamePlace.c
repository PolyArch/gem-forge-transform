#define N 2

int p;

int *bar() { return &p; }

void foo(int *a) {
  for (int j = 0; j < N; ++j) {
    int *p = bar();
    for (int i = 0; i < N; ++i) {
      *p = a[i];
    }
  }
}

int main() {
  int a[N];
  int *pp[N];
  for (int i = 0; i < N; ++i) {
    pp[i] = (int *)malloc(sizeof(int));
  }
  foo(a);
  return 0;
}