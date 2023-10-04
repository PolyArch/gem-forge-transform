/**
 * Simple array dot prod.
 */
#include "gfm_utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "immintrin.h"

typedef ValueT Value;

#define STRIDE 1

/**
 * Parameters:
 * STATIC_CHUNK_SIZE: OpenMP static scheduling chunk size.
 */
#ifndef STATIC_CHUNK_SIZE
#define STATIC_CHUNK_SIZE 0
#endif

__attribute__((noinline)) Value foo(int32_t *indA, int32_t *indB, float *valA,
                                    float *valB, int aSize, int bSize) {

  float valM = 0;
  int i = 0;
  int j = 0;

  while (i < aSize && j < bSize) {

    // printf("i %d j %d.\n", i, j);

    int32_t idxA = indA[i];
    int32_t idxB = indB[i];

    valM = (idxA == idxB) ? valM + (valA[i] * valB[j]) : valM;
    i = idxA < idxB ? i + 1 : i;
    j = idxB < idxA ? j + 1 : j;
  }
  printf("Result = %f\n", valM);
  return 0;
}

int main(int argc, char *argv[]) {

  int numThreads = 1;
  uint64_t offsetBytes = 0;
  uint64_t N = 16 * 1024 * 1024 / sizeof(Value);
  int check = 0;
  int warm = 0;
  int argx = 2;
  if (argc >= argx) {
    numThreads = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    N = atoll(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    check = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    warm = atoi(argv[argx - 1]);
  }
  argx++;
  if (argc >= argx) {
    offsetBytes = atoll(argv[argx - 1]);
  }
  argx++;
  int random = 0;
  if (offsetBytes == -1) {
    random = 1;
    offsetBytes = 0;
  }
  printf("Number of Threads: %d.\n", numThreads);
  printf("Data size %lukB Offset %lukB Random %d.\n", N * sizeof(Value) / 1024,
         offsetBytes / 1024, random);

  gf_detail_sim_start();
  printf("Post Detail Sim Start\n");

  gf_reset_stats();
  printf("Post Reset Stats\n");
  int32_t indA[] = {0, 1, 2};
  int32_t indB[] = {1, 2, 5, 8};
  float valA[] = {3, 4, 5};
  float valB[] = {100, 2, 0, 4};
  int sizeA = sizeof(valA) / sizeof(valA[0]);
  int sizeB = sizeof(valB) / sizeof(valB[0]);
  printf("Pre-Foo Calling %d %d \n", sizeA, sizeB);
  volatile Value computed = foo(indA, indB, valA, valB, sizeA, sizeB);
  printf("Foo has now completed!\n");
  gf_detail_sim_end();
  return 0;
}