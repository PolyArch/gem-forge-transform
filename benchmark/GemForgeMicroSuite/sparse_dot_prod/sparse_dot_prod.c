/**
 * Simple write after read for memory accesses.
 */
#include "gem5/m5ops.h"

#include <stdio.h>
#include <stdlib.h>

typedef long long Value;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo(Value *a, Value *b, int *ia, int *ib,
                                    int N) {
  int x = 0;
  int y = 0;
  Value c = 0;
  while (x < N && y < N) {
    if (ia[x] == ib[y]) {
      c += a[x] * b[y];
      x++;
      y++;
    } else if (ia[x] < ib[y]) {
      x++;
    } else {
      y++;
    }
  }
  return c;
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];
Value b[N];
int ia[N];
int ib[N];

void fillIndex();

int main() {
  fillIndex();

#ifdef WARM_CACHE
  for (int i = 0; i < N; ++i) {
    a[i] = i;
    b[i] = i;
  }
#endif

  int ia[] = {0, 2, 4, 6, 8};
  int ib[] = {1, 2, 3, 8, 9};
  m5_detail_sim_start();
  volatile Value ret = foo(a, b, ia, ib, N);
  m5_detail_sim_end();

#ifdef CHECK
  {
    int x = 0;
    int y = 0;
    Value expected = 0;
    while (x < N && y < N) {
      if (ia[x] == ib[y]) {
        expected += a[x] * b[y];
        x++;
        y++;
      } else if (ia[x] < ib[y]) {
        x++;
      } else {
        y++;
      }
    }
    printf("Ret = %llu, Expected = %llu.\n", ret, expected);
  }
#endif
  return 0;
}

void fillIndex() {
  int indexA = 0;
  int indexB = 0;
  for (int i = 0; i < N; ++i) {
    if (((float)rand()) / ((float)RAND_MAX) < 0.3f) {
      // This is forced to match to the larger one.
      ia[i] = indexA > indexB ? indexA : indexB;
      ib[i] = indexA > indexB ? indexA : indexB;
      // Increment by at least one.
      indexA = indexB = (ia[i] + 1);
    } else {
      // Not forced to match. Increased at [1, 6)
      ia[i] = indexA;
      ib[i] = indexB;
      int deltaA = (rand() % 5) + 1;
      int deltaB = (rand() % 5) + 1;
      indexA += deltaA;
      indexB += deltaB;
    }
  }
}