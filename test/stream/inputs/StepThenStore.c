#define N 2

void foo(int *arr) {
  int nvisited = 0;
  for (int i = 0; i < N;) {
    arr[nvisited] = ++i;
    nvisited++;
  }
}

int main() {
  int arr[N];
  foo(arr);
  return 0;
}