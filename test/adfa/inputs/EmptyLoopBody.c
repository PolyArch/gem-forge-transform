/**
 * A very simple loop body with no loop-carry dependence.
 */

#define N 16

int foo(volatile int* a) {
    int b = 0;
    for (int i = 0; i < N; ++i) {
      b = *a;
    }
    return b;
}

int main() {
    int a = 0;
    int b = foo(&a);
    return 0;
}