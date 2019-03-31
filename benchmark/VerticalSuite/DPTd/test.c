#include <stdio.h>
#include "randArr.h"
#include "common.h"
#include "math.h"

#define ASIZE   128
#define ITERS    16

double arrA[ASIZE];
double arrB[ASIZE];

__attribute__ ((noinline))
double loop(int zero) {
  int i, iters;
  double t1;

  for(iters=zero; iters < ITERS; iters+=1) {
    for(i=zero; i < ASIZE; i+=1) {
      arrA[i]=sin(arrA[i]);
    }
    t1+=arrA[ASIZE-1];
  }

  return t1;
}

int main(int argc, char* argv[]) {
   argc&=10000;
   DETAILED_SIM_START(); 
   int t=loop(argc); 
   DETAILED_SIM_STOP();
   volatile double a = t;
}