
#define N 9

char *list[N];
int element_size;

char **bar(char *p) { return (char **)p; }

void foo() {
  char *p = (char *)(&(list[N - 1]));
  /**
   * bar() is used to force clang make pp a phi node in the loop.
   */
  char **pp = bar(p);
  int size = element_size;
  for (int i = N - 2; i >= 0; i -= 2) {
    p -= size;
    *pp = p;
    pp = (char **)p;
    p -= size;
    *pp = p;
    pp = (char **)p;
  }
}

int main() {
  element_size = sizeof(list[0]);
  foo();
  return 0;
}