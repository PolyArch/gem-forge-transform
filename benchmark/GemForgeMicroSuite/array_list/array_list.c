#include "../gem5_pseudo.h"
#include <stdlib.h>

// Simple pointer chasing access of a list implement using array.
typedef int Value;
typedef uint64_t Index;

__attribute__((noinline)) Value foo(Index *next, Index pos) {
  Value sum = 0.0;
#pragma nounroll
  while (pos != -1) {
    sum += 1.0;
    pos = next[pos];
  }
  return sum;
}

// 65536 * 2 * 4 = 512kB
const Index N = 65536 * 2;
Index ia[N];
Index next[N];

int main() {
  // Initialize the index array.
  for (Index i = 0; i < N; ++i) {
    // ia[i] = (int)(((float)(rand()) / (float)(RAND_MAX)) * N);
    ia[i] = i;
  }

  // Shuffle it.
  for (Index j = N - 1; j > 0; --j) {
    Index i = (Index)(((float)(rand()) / (float)(RAND_MAX)) * j);
    Index tmp = ia[i];
    ia[i] = ia[j];
    ia[j] = tmp;
  }

  // Connect them in the next.
  for (Index i = 0; i < N - 1; ++i) {
    next[ia[i]] = ia[i + 1];
  }
  next[ia[N - 1]] = -1;

  volatile Value ret;
  DETAILED_SIM_START();
  ret = foo(next, ia[0]);
  DETAILED_SIM_STOP();
  return 0;
}