#include <stdio.h>
#include "randArr.h"
#include "common.h"

#define ASIZE 131072
#define STEP     512
#define ITERS    256

int arr[ASIZE];

__attribute__ ((noinline))
int loop(int zero) {
  int t = 0,i,iter;
  for(iter=0; iter < ITERS; ++iter) {
    for(i=iter; i < ASIZE; i+=STEP) {
      arr[i]=i;
    }
    t+=arr[zero];
  }
  return t+arr[zero];
}

int main(int argc, char* argv[]) {
   argc&=10000;
   DETAILED_SIM_START(); 
   int t=loop(argc); 
   DETAILED_SIM_STOP();
   volatile int a = t;
}
