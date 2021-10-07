
/**
 * A very simple loop body with no loop-carry dependence.
 */

#define N 16

int foo(volatile int* a, volatile int* aa) {
  int b = 0;
  int c = 0;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < 2; ++j) {
      b += *a;
    }
    c += *aa;
  }
  return b + c;
}

int main() {
  int a = 0;
  int b = foo(&a, &a);
  return 0;
}