/**
 * Simple write after read for memory accesses.
 */
#include "gem5/m5ops.h"

#include <stdio.h>
#include <stdlib.h>

typedef long long Value;
typedef int32_t Index;

#define CHECK
#define WARM_CACHE

__attribute__((noinline)) Value foo_branch(Value *a, Value *b, Index *ia,
                                           Index *ib, int M, int N) {
  int64_t x = 0;
  int64_t y = 0;
  Value c = 0;
  while (x < M && y < N) {
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

__attribute__((noinline)) Value foo(Value *a, Value *b, Index *ia, Index *ib,
                                    int M, int N) {
  int64_t x = 0;
  int64_t y = 0;
  Value c = 0;
  while (x < M && y < N) {
    int idxA = ia[x];
    int idxB = ib[y];
    Value valA = a[x];
    Value valB = b[y];
    Value nextC = valA * valB + c;
    c = (idxA == idxB) ? nextC : c;
    x = (idxA <= idxB) ? (x + 1) : x;
    y = (idxB <= idxA) ? (y + 1) : y;
  }
  return c;
}

// 65536*4 is 512kB.
const int N = 65536;
Value a[N];
Value b[N];
Index ia[N];
Index ib[N];

void fillIndex();

int main() {
  fillIndex();

#ifdef WARM_CACHE
  for (int i = 0; i < N; ++i) {
    a[i] = i;
    b[i] = i;
  }
#endif

  m5_detail_sim_start();
  volatile Value ret = foo(a, b, ia, ib, N, N);
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