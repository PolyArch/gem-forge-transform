#include <stdio.h>
#include <stdlib.h> /* malloc, free, rand */

#include "common.h"
#include "randArr.h"

#define ASIZE 65536
#define STEP 128
#define ITERS 4096
#define LEN 2048

typedef struct dude {
  int p1, p2, p3, p4;
} dude;

// dude arr[ASIZE];
__attribute__((noinline)) int loop(int zero, dude *arr) {
  int t = 0, count = 0;

  unsigned lfsr = 0xACE1u;
  do {
    /* taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
    lfsr = lfsr + arr[lfsr].p1;
  } while (lfsr != 0xACE1u && ++count < ITERS);

  return lfsr;
}

int main(int argc, char *argv[]) {
  argc &= 10000;
  dude *arr = calloc(ASIZE, sizeof(dude));
  for (int i = 0; i < ASIZE; ++i) {
    arr[i].p1 = i;
  }
  m5_detail_sim_start();
  int t = loop(argc, arr);
  m5_detail_sim_end();
  volatile int a = t;
  printf("result %d.\n", a);
  free(arr);
}
